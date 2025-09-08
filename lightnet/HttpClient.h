#include "HttpConnectionPool.h"
#include <map>
#include <queue>

class HttpClient : public std::enable_shared_from_this<HttpClient> {
public:
  using ResponseHandler =
      std::function<void(const http::response<http::string_body> &,
                         const boost::system::error_code &)>;

  HttpClient(std::shared_ptr<boost::asio::io_context> ioc,
             const std::string &host, const std::string &port,
             const std::string &target, http::verb method,
             const std::string &body,
             const std::map<std::string, std::string> &headers,
             ResponseHandler handler, size_t pool_size = 8);

  HttpClient(std::shared_ptr<boost::asio::io_context> ioc,
             const std::string &host, const std::string &port,
             const std::string &target, http::verb method,
             const std::string &body,
             const std::map<std::string, std::string> &headers,
             size_t pool_size = 8);
  // 一次发送请求：支持 GET/POST/PUT/DELETE
  void send_request();

  void send_request(ResponseHandler handler);

private:
  // 连接池资源
  std::shared_ptr<boost::asio::io_context> ioc_;
  std::shared_ptr<HttpConnectionPool> connection_pool_;
};