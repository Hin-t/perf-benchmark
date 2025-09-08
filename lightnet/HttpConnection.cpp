#include "HttpConnection.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>
#include <chrono>

using tcp = boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

HttpConnection::HttpConnection(
    std::shared_ptr<boost::asio::io_context> ioc, const std::string &host,
    const std::string &port, const std::string &target, http::verb method,
    const std::string &body, const std::map<std::string, std::string> &headers,
    ResponseHandler handler)
    : resolver_(*ioc), host_(host), port_(port), target_(target),
      method_(method), body_(body), headers_(headers), stream_(*ioc),
      timer_(*ioc), handler_(handler) {}

HttpConnection::HttpConnection(
    std::shared_ptr<boost::asio::io_context> ioc, const std::string &host,
    const std::string &port, const std::string &target, http::verb method,
    const std::string &body, const std::map<std::string, std::string> &headers)
    : resolver_(*ioc), host_(host), port_(port), target_(target),
      method_(method), body_(body), headers_(headers), stream_(*ioc),
      timer_(*ioc) {}

void HttpConnection::async_connect() {
  resolver_.async_resolve(
      host_, port_,
      [this](beast::error_code ec, tcp::resolver::results_type results) {
        if (!ec) {
          stream_.async_connect(
              results, [self = shared_from_this()](
                           beast::error_code ec,
                           const tcp::resolver::results_type::endpoint_type &) {
                if (!ec) {
                  std::cerr << "连接成功! " << std::endl;
                } else {
                  std::cerr << "连接失败: " << ec.message() << std::endl;
                }
              });
        }
      });
}

void HttpConnection::sync_connect() {
  beast::error_code err;
  auto results = resolver_.resolve(host_, port_, err);
  if (!err) {
  }
  stream_.connect(results);
  std::cerr << "连接成功! " << std::endl;
  is_connected_ = true;
}



void HttpConnection::async_write_request(ResponseHandler handler) {
  req_.version(11);
  req_.method(method_);
  req_.target(target_);
  req_.set(http::field::host, host_);
  req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req_.keep_alive(true);
  for (const auto &h : headers_)
    req_.set(h.first, h.second);
  if (!body_.empty()) {
    req_.body() = body_;
    req_.prepare_payload();
  }

  // 记录起始时间
  auto start = std::chrono::high_resolution_clock::now();

  http::async_write(
      stream_, req_,
      [handler, self = shared_from_this(), start](beast::error_code ec,
                                                  std::size_t) {
        // 统计耗时
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();
        std::cout << "async_write 耗时: " << duration << " 微秒" << std::endl;

        if (!ec) {
          self->async_read_response(handler);
        } else if (self->handler_) {
          std::cout << "write failed! " << ec << std::endl;
          self->handler_(self->res_, ec);
        }
      });
}

void HttpConnection::async_read_response(ResponseHandler handler,
                                         std::chrono::milliseconds timeout) {
  // 启动超时定时器
  timer_.expires_after(timeout);
  auto self = shared_from_this();
  timer_.async_wait([self](const beast::error_code &ec) {
    if (!ec) {
      // 定时器到期，取消流上所有操作
      self->stream_.socket().cancel();
    }
  });

  // 记录起始时间
  auto start = std::chrono::high_resolution_clock::now();
  http::async_read(
      stream_, buffer_, res_,
      [handler, self, start](beast::error_code ec, std::size_t) {
        self->timer_.cancel(); // 无论如何，读完都停止定时器
        if (ec == boost::asio::error::operation_aborted) {
          // 超时或被取消
          if (self->handler_)
            self->handler_(self->res_,
                           beast::error_code(boost::asio::error::timed_out));
        } else {
          if (handler)
            handler(self->res_, ec);
        }
        // 统计耗时
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();
        std::cout << "async_read 耗时: " << duration << " 微秒" << std::endl;
        // 异步关闭（也可继续串联后续操作，比如 pipeline）
        //  beast::error_code ec_shutdown;
        //  self->stream_.socket().shutdown(tcp::socket::shutdown_both,
        //                                  ec_shutdown);
        // 后续操作继续异步写/下一个请求时，也可继续调用其它异步操作
      });
}

http::response<http::string_body>
HttpConnection::sync_write_request(std::chrono::milliseconds timeout) {
  beast::error_code err;
  // 1. 解析域名
  if (!is_connected_) {
    sync_connect();
  }
  // 3. 构造请求
  http::request<http::string_body> req;
  req.version(11);
  req.method(method_);
  req.target(target_);
  req.set(http::field::host, host_);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.keep_alive(true);
  for (const auto &h : headers_)
    req.set(h.first, h.second);
  if (!body_.empty()) {
    req.body() = body_;
    req.prepare_payload();
  }
  // 4. 发送请求
  http::write(stream_, req, err);
  if (err) {
    return {};
  }
  // 5. 读取响应
  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  stream_.expires_after(timeout);
  http::read(stream_, buffer, res, err);
  // 6. 安全关闭
  // beast::error_code shutdown_ec;
  // stream_.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
  // is_connected_ = false;

  return res;
}

void HttpConnection::send_request(ResponseHandler handler) {
  if (!is_connected_) {
    sync_connect();
  }
  async_write_request(handler);
}