#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <map>
#include <memory>

using tcp = boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
  using ResponseHandler =
      std::function<void(const http::response<http::string_body> &,
                         const boost::system::error_code &)>;
  // async
  HttpConnection(std::shared_ptr<boost::asio::io_context> ioc,
                 const std::string &host, const std::string &port,
                 const std::string &target, http::verb method,
                 const std::string &body,
                 const std::map<std::string, std::string> &headers,
                 ResponseHandler handler);
  // sync
  HttpConnection(std::shared_ptr<boost::asio::io_context> ioc,
                 const std::string &host, const std::string &port,
                 const std::string &target, http::verb method,
                 const std::string &body,
                 const std::map<std::string, std::string> &headers);

  void sync_connect();

  void async_connect();

  void async_write_request(ResponseHandler handler);

  void async_read_response(
      ResponseHandler handler,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(100));

  http::response<http::string_body> sync_write_request(
      std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

  void send_request(ResponseHandler handler);

private:
  tcp::resolver resolver_;
  // 真实连接
  beast::tcp_stream stream_;
  boost::asio::steady_timer timer_;
  http::request<http::string_body> req_;
  beast::flat_buffer buffer_;
  http::response<http::string_body> res_;
  ResponseHandler handler_;
  std::string host_, target_, body_, port_;
  http::verb method_;
  std::map<std::string, std::string> headers_;

  bool is_connected_;
};