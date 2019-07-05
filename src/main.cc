#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "insight-app.hh"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
gltf_insight::app* g_app = nullptr;
void run_frame()
{
  g_app->main_loop_frame();
}
#endif

int main(int argc, char** argv) {
#ifdef __EMSCRIPTEN__
  std::cout <<"This program is running under emscripten\n";
#endif
  gltf_insight::app application{argc, argv};
  std::cout << "Created gltf_insight::app object\n";
#ifndef __EMSCRIPTEN__
  application.main_loop();
#else
  g_app = &application;
  emscripten_set_main_loop(run_frame, 0, 1);
#endif
  return EXIT_SUCCESS;
}
