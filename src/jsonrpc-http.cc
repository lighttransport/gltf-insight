#include "jsonrpc-http.hh"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#if defined(GLTF_INSIGHT_WITH_JSONRPC)

#include "CivetServer.h"
#endif

#include <fmt/core.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <iostream>
#include <vector>
#include <thread>

namespace gltf_insight {

#if defined(GLTF_INSIGHT_WITH_JSONRPC)

class JHandler : public CivetHandler {
 public:
  JHandler(std::atomic<bool>* exit_flag,
           std::function<void(const std::string&)> callback)
      : exit_flag_(exit_flag), callback_(std::move(callback)) {}

  virtual ~JHandler();

   private:

 public:
  bool handlePost(CivetServer* server, struct mg_connection* conn) {
    /* Handler may access the request info using mg_get_request_info */
    const struct mg_request_info* req_info = mg_get_request_info(conn);
    long long rlen;
    long long nlen = 0;
    long long tlen = req_info->content_length;
    char buf[1024];

    std::string post_str;

    // Read post data(data may be sent in chunks).
    while (nlen < tlen) {
      rlen = tlen - nlen;
      if (rlen > static_cast<long long>(sizeof(buf))) {
        rlen = sizeof(buf);
      }
      rlen = mg_read(conn, buf, size_t(rlen));
      if (rlen <= 0) {
        break;
      }
      post_str += std::string(buf, size_t(rlen));
      nlen += rlen;
    }
	fmt::print("post : {}", post_str);

    callback_(post_str);

    (void)server;
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: "
              "application/json-rpc\r\n\r\n");
    mg_printf(conn, "{\"jsonrpc\":\"2.0\", \"result\": 0, \"id\": 0}"); // TODO(LTE): result and response id
    return true;
  }

  std::atomic<bool>* exit_flag_;
  std::function<void(const std::string&)> callback_;
};

JHandler::~JHandler() {}

#endif


bool JSONRPC::listen_blocking(
    std::function<void(const std::string&)> callback,
    std::atomic<bool>* exit_flag, const std::string address,
    const int port)
{
#if defined(GLTF_INSIGHT_WITH_JSONRPC)

  std::vector<std::string> options;
  options.push_back("listening_ports");
  options.push_back(std::to_string(port));

  std::unique_ptr<CivetServer> server;
  try {
    server.reset(new CivetServer(options));
  } catch (std::exception &e) {
    fmt::print("CiverServer run error: what = {}\n", e.what());

    return false;
  }

  fmt::print("Listen on {}:{}\n", address, port);
  std::cout << std::flush;

  JHandler h_j(exit_flag, callback);
  server->addHandler("/v1", h_j);

  while (!(*exit_flag)) {

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return true;
#else

  fmt::print("JSONRPC feature is not enabled in this build.\n");
  (void)address;
  (void)port;
  (void)callback;
  (void)exit_flag;
  return false;
#endif
}

} // namespace gltf_insight
