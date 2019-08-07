#include "os_utils.hh"

#include <cstdlib>
#include <iostream>
#include <fstream>

#if defined(ANDROID)
#elif defined(_MSC_VER)
#else // Assume linux or macOS
#include <wordexp.h>
#endif

// Platform detection macros :
#if defined(_WIN32)
#define OS_UTILS_WINDOWS
#endif
#if defined(__linux__)
#define OS_UTILS_LINUX
#define OS_UTILS_UNIX
#endif
#if defined(__APPLE__)
#define OS_UTILS_APPLE
#define OS_UTILS_UNIX
#endif
#if defined(_EMSCRIPTEN_)
#define OS_UTILS_WEB
#define OS_UTILS_UNIX  // posix APIs are emulated
#endif

std::string os_utils::platform() {
#if defined(OS_UTILS_WINDOWS)
  return "MS Windows";
#elif defined(OS_UTILS_LINUX)
  return "GNU/Linux";
#elif defined(OS_UTILS_APPLE)
  // TODO detect if macOs or iOs ?
  return "Apple UNIX";
#elif defined(OS_UTILS_WEB)
  return "Web";
#elif defined(OS_UTILS_UNIX)
  return "UNIX-like";
#else
  return "unknown";
#endif
}

// mkdir
#ifdef OS_UTILS_WINDOWS
#include <direct.h>
#endif
#ifdef OS_UTILS_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#endif
bool os_utils::mkdir(const std::string& dir_path) {
  int status = 0;

#ifdef OS_UTILS_WINDOWS
  status = _mkdir(dir_path.c_str());  // can be used on Windows
#endif

#ifdef OS_UTILS_UNIX
  mode_t dir_mode = 0733;                        // UNIX style permissions
  status = ::mkdir(dir_path.c_str(), dir_mode);  // can be used on non-Windows
#endif

  if (status != 0 && errno != EEXIST) {
    (void)status;
    std::cerr << "We attempted to create directory " << dir_path
              << " And there's an error that is not EEXIST\n";
    return false;
  }

  return true;
}

bool os_utils::mkdir_from_filepath(const std::string& file_path) {
  const std::string path_to_last_dir =
      file_path.substr(0, file_path.find_last_of("/\\"));

  return mkdir(path_to_last_dir);
}

// end of mkdir

// open_url
#if defined(OS_UTILS_WINDOWS)
#include <Windows.h>
#include <shellapi.h>
#elif defined(OS_UTILS_APPLE)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CFBundle.h>
#endif

bool os_utils::open_url(const std::string& url) {
#if defined(OS_UTILS_WINDOWS)
  (void)ShellExecuteA(nullptr, nullptr, url.c_str(), nullptr, nullptr, SW_SHOW);
#elif defined(OS_UTILS_LINUX)
  std::string command = "xdg-open " + url;
  (void)std::system(command.c_str());
#elif defined(OS_UTILS_APPLE)
  CFURLRef cfurl = CFURLCreateWithBytes(
      nullptr,                                      // allocator
      reinterpret_cast<const UInt8*>(url.c_str()),  // URLBytes
      static_cast<long>(url.length()),              // length
      kCFStringEncodingASCII,                       // encoding
      nullptr                                       // baseURL
  );
  LSOpenCFURLRef(cfurl, nullptr);
  CFRelease(cfurl);
#else
  std::cerr
      << "Warn: Cannot open URLs on this platform. We should have displayed "
      << url << ". Please send bug request on github\n";
  return false;
#endif

  return true;
}
// end of open_url

std::string os_utils::expand_filepath(const std::string &filepath) {
#ifdef _WIN32
  DWORD len = ExpandEnvironmentStringsA(filepath.c_str(), NULL, 0);
  char *str = new char[len];
  ExpandEnvironmentStringsA(filepath.c_str(), str, len);

  std::string s(str);

  delete[] str;

  return s;
#else

#if defined(ANDROID)
  // no expansion
  std::string s = filepath;
#else
  std::string s;
  wordexp_t p;

  if (filepath.empty()) {
    return "";
  }

  // char** w;
  int ret = wordexp(filepath.c_str(), &p, 0);
  if (ret) {
    std::cerr << "Filepath expansion err: " << ret << "\n";
    // err
    s = filepath;
    return s;
  }

  // Use first element only.
  if (p.we_wordv) {
    s = std::string(p.we_wordv[0]);
    wordfree(&p);
  } else {
    s = filepath;
  }

#endif

  return s;
#endif
}


bool os_utils::file_exists(const std::string &filepath) {
  std::ifstream ifs(filepath);

  if (!ifs) {
    return false;
  }

  return true;
}



