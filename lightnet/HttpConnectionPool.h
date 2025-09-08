#pragma once
#include "HttpConnection.h"
#include <boost/asio.hpp>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

class HttpConnectionPool
    : public std::enable_shared_from_this<HttpConnectionPool> {
public:
  using ConnectionPtr = std::shared_ptr<HttpConnection>;
  using ResponseHandler = HttpConnection::ResponseHandler;

  HttpConnectionPool(std::shared_ptr<boost::asio::io_context> ioc,
                     const std::string &host, const std::string &port,
                     const std::string &target, http::verb method,
                     const std::string &body,
                     const std::map<std::string, std::string> &headers,
                     size_t pool_size);
  HttpConnectionPool(std::shared_ptr<boost::asio::io_context> ioc,
                     const std::string &host, const std::string &port,
                     const std::string &target, http::verb method,
                     const std::string &body,
                     const std::map<std::string, std::string> &headers,
                     ResponseHandler handler, size_t pool_size);

  void async_request(ResponseHandler handler);

  void sync_request(std::shared_ptr<boost::asio::io_context> ioc);

private:
  void release(std::shared_ptr<HttpConnection> connection);

  std::shared_ptr<boost::asio::io_context> ioc_;
  size_t pool_size_;
  std::mutex mutex_;
  std::queue<std::shared_ptr<HttpConnection>> idle_;
  std::vector<std::shared_ptr<HttpConnection>> all_; // 持有，避免释放
};