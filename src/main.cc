
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "glad/include/glad/glad.h"

#include "GLFW/glfw3.h"

static void error_callback(int error, const char *description) {
  std::cerr << "GLFW Error : " << error << ", " << description << std::endl;
}


int main(int argc, char **argv)
{

  // Setup window
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) return false;
  GLFWwindow *window = glfwCreateWindow(1600, 900, "Liselotte GUI", NULL, NULL);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable vsync

  // glad must be called after glfwMakeContextCurrent()

  if (!gladLoadGL()) {
    std::cerr << "Failed to initialize OpenGL context." << std::endl;
    return false;
  }

  if (((GLVersion.major == 2) && (GLVersion.minor >= 1)) || (GLVersion.major >= 3)) {
    // ok
  }
  else {
    std::cerr << "OpenGL 2.1 is not available." << std::endl;
    return false;
  }

  return EXIT_SUCCESS;
}
