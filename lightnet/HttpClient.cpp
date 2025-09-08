#include "HttpClient.h"

HttpClient::HttpClient(std::shared_ptr<boost::asio::io_context> ioc,
                       const std::string &host, const std::string &port,
                       const std::string &target, http::verb method,
                       const std::string &body,
                       const std::map<std::string, std::string> &headers,
                       ResponseHandler handler, size_t pool_size)
    : ioc_(ioc) {
  connection_pool_ = std::make_shared<HttpConnectionPool>(
      ioc, host, port, target, method, body, headers, handler, pool_size);
}

HttpClient::HttpClient(std::shared_ptr<boost::asio::io_context> ioc,
                       const std::string &host, const std::string &port,
                       const std::string &target, http::verb method,
                       const std::string &body,
                       const std::map<std::string, std::string> &headers,
                       size_t pool_size)
    : ioc_(ioc) {
  connection_pool_ = std::make_shared<HttpConnectionPool>(
      ioc, host, port, target, method, body, headers, pool_size);
}

void HttpClient::send_request(ResponseHandler handler) {
  connection_pool_->async_request(handler);
}

void HttpClient::send_request() { connection_pool_->sync_request(ioc_); }