#include "00Names.hpp"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#define stat _stat
#define popen _popen
#define pclose _pclose
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

uint64_t lastModified(const std::string& filename) {
  struct stat result;
  if (stat(filename.c_str(), &result) != 0) {
    return 0;
  }
  return result.st_mtime;
}

bool execute(std::string command) {
  if (command[0] == '@') command.erase(command.begin());
  else puts(command.c_str());
  return system(command.c_str()) == 0;
}

std::filesystem::path getProgramPath() {
  char buffer[1024];
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
  GetModuleFileName(nullptr, buffer, sizeof(buffer));
#else
  (void)readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif
  return std::filesystem::path(buffer);
}
