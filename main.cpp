#include "lightnet/HttpClient.h"
// #include "lightnet/HttpConnection.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>

using tcp = boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

void send_loop(std::shared_ptr<HttpClient> client, boost::asio::io_context &io,
               int total_count, int interval_ms, int &sent_count,
               int pool_size) {
  if (sent_count >= total_count)
    return;

  auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < pool_size; ++i) {
    client->send_request();
  }
  // client->send_request(); // 只统计这句的耗时

  auto end = std::chrono::steady_clock::now();
  auto cost = std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                  .count();
  std::cout << "send_request() cost: " << cost << " us" << std::endl;

  ++sent_count;

  auto timer = std::make_shared<boost::asio::steady_timer>(
      io, std::chrono::milliseconds(interval_ms));
  timer->async_wait([client, &io, total_count, interval_ms, &sent_count, timer,
                     pool_size](const boost::system::error_code &) {
    send_loop(client, io, total_count, interval_ms, sent_count, pool_size);
  });
}

void sync_test() {
  // 每个线程/客户端独享自己的 io_context 和 HttpClient
  int thread_num = 1;
  int total_count = 2500000; // 每个客户端请求数
  int interval_ms = 1;       // 每隔 1 ms 发送一次
  int pool_size = 64;
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; ++i) {
    threads.emplace_back([=]() {
      auto ioc = std::make_shared<boost::asio::io_context>();
      auto client = std::make_shared<HttpClient>(
          ioc, "127.0.0.1", "18100", "/", boost::beast::http::verb::post,
          R"({"method":"eth_chainId","params":[],"id":1,"jsonrpc":"2.0"})",
          std::map<std::string, std::string>{
              {"Content-Type", "application/json"}},
          pool_size // connection pool size
      );
      int sent_count = 0;
      send_loop(client, *ioc, total_count, interval_ms, sent_count, pool_size);
      ioc->run();
    });
  }
  for (auto &t : threads)
    t.join();
}

void async_connection_test() {
  auto ioc = std::make_shared<boost::asio::io_context>();

  auto timer = std::make_shared<boost::asio::steady_timer>(*ioc);

  std::vector<std::shared_ptr<HttpConnection>> conns;
  for (int i = 0; i < 1024; ++i) {
    auto conn = std::make_shared<HttpConnection>(
        ioc, "127.0.0.1", "18100", "/", boost::beast::http::verb::post,
        R"({"method":"eth_chainId","params":[],"id":1,"jsonrpc":"2.0"})",
        std::map<std::string, std::string>{
            {"Content-Type", "application/json"}},
        nullptr);
    conns.push_back(conn);
  }
  std::function<void()> loop;
  loop = [conns, timer, &loop]() {
    for (auto &conn : conns) {
      conn->send_request(
          [conn, timer, &loop](const http::response<http::string_body> &resp,
                               const boost::system::error_code &err) {
            // std::cout << resp.body() << std::endl;

            timer->expires_after(std::chrono::nanoseconds(500));
            timer->async_wait([&loop](const boost::system::error_code &ec) {
              if (!ec)
                loop(); // 继续下一轮
            });
          });
    }
  };

  loop();
  ioc->run();
}

void async_connection_pool_test() {
  auto ioc = std::make_shared<boost::asio::io_context>();
  auto conn_pool = std::make_shared<HttpConnectionPool>(
      ioc, "127.0.0.1", "18100", "/", boost::beast::http::verb::post,
      R"({"method":"eth_chainId","params":[],"id":1,"jsonrpc":"2.0"})",
      std::map<std::string, std::string>{{"Content-Type", "application/json"}},
      nullptr, 8);

  conn_pool->async_request(
      [](const http::response<http::string_body> &resq,
         const boost::system::error_code &err) { std::cout << resq.body(); });

  ioc->run();
}
int main() {
  // async_connection_test();
  async_connection_pool_test();
  return 0;
}