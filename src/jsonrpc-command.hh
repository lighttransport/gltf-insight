#ifndef GLTF_INSIGHT_JSONRPC_COMMAND_HH_
#define GLTF_INSIGHT_JSONRPC_COMMAND_HH_

namespace gltf_insight {

struct Command
{
 public:

  enum Type {
    MORPH_WEIGHT,
  };

  Type type;

  // for MORPH_WEIGHT
  std::pair<int, float> morph_weight; // <target_id, weight>

};

} // gltf_insight

#endif // GLTF_INSIGHT_JSONRPC_COMMAND_HH_
