#ifndef GLTF_INSIGHT_SRC_JSONRPC_HTTP_HH_
#define GLTF_INSIGHT_SRC_JSONRPC_HTTP_HH_

///
/// JSON-RPC over HTTP to communiate with other applications.
/// For example, you can change morph weights from other application.
///
/// TODO:
///   [ ] Definie JSON schema for JSON-RPC message
///   [ ] Send a messsage from gltf-insight to other application.
///

#include <string>
#include <atomic>
#include <functional>

namespace gltf_insight {

class JSONRPC {
 public:
   JSONRPC() : _addr("localhost"), _port(21264) {}

   /// Start http server. The method is blocking operation and goes to infinite loop.
   /// application must invoke `listen_blockking` method by creating a dedicated thread.
   /// To terminate the listen loop, other thread set `exit_flag` to true(TODO(LTE): USE conditional?)
   ///
   /// @param[in] callback Callback function called for each HTTP request. The argument passed contains JSON string.
   /// @param[in] exit_flag When this value is set to true, the method exits listen loop
   ///
   bool listen_blocking(
       std::function<void(const std::string&)> callback,
       std::atomic<bool>* exit_flag,
	   const std::string address = "localhost",
                        const int port = 21264);

 private:
  std::string _addr = "localhost";
  int _port = 21264;

};

} // gltf_insight


#endif // GLTF_INSIGHT_SRC_JSONRPC_HTTP_HH_
