#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "insight-app.hh"

int main(int argc, char** argv) {
  gltf_insight::app application{argc, argv};

  application.main_loop();

  return EXIT_SUCCESS;
}
