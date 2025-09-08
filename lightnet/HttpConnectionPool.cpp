#include "HttpConnectionPool.h"
#include <boost/asio.hpp>

HttpConnectionPool::HttpConnectionPool(
    std::shared_ptr<boost::asio::io_context> ioc, const std::string &host,
    const std::string &port, const std::string &target, http::verb method,
    const std::string &body, const std::map<std::string, std::string> &headers,
    ResponseHandler handler, size_t pool_size)
    : ioc_(ioc), pool_size_(pool_size) {
  for (size_t i = 0; i < pool_size_; ++i) {
    auto connection = std::make_shared<HttpConnection>(
        ioc_, host, port, target, method, body, headers, handler);
    idle_.push(connection);
    all_.emplace_back(connection);
  }
}

HttpConnectionPool::HttpConnectionPool(
    std::shared_ptr<boost::asio::io_context> ioc, const std::string &host,
    const std::string &port, const std::string &target, http::verb method,
    const std::string &body, const std::map<std::string, std::string> &headers,
    size_t pool_size)
    : ioc_(ioc), pool_size_(pool_size) {
  for (size_t i = 0; i < pool_size_; ++i) {
    auto connection = std::make_shared<HttpConnection>(ioc_, host, port, target,
                                                       method, body, headers);
    idle_.push(connection);
    all_.emplace_back(connection);
  }
}

void HttpConnectionPool::async_request(ResponseHandler handler) {
  std::shared_ptr<HttpConnection> connection;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connection = idle_.front();
    idle_.pop();
  }
  auto self = shared_from_this();
  auto wrap_handler = [this, self, connection, handler](const auto &res,
                                                        const auto &ec) {
    handler(res, ec);
    this->release(connection);
  };
  connection->send_request(handler);
}

void HttpConnectionPool::sync_request(
    std::shared_ptr<boost::asio::io_context> ioc) {
  std::shared_ptr<HttpConnection> connection;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connection = idle_.front();
    idle_.pop();
  }

  connection->sync_write_request();
  release(connection);
}

void HttpConnectionPool::release(std::shared_ptr<HttpConnection> connection) {
  std::lock_guard<std::mutex> lock(mutex_);
  idle_.push(connection);
}