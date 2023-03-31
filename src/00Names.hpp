#pragma once
#include <filesystem>
#include <string>
#include <vector>

extern std::filesystem::path libdirPath;

/*          WILDCARD          */
std::string replace(std::string str, const std::string& from, const std::string& to);
bool wildcardMatch(const std::string& str, const std::string& pattern);
std::vector<std::string> wildcard(const std::string& pattern, bool dir = false);

/*          PACKAGE MANAGER          */
void searchPackage(const std::string& name);
void installPackage(const std::string& name);

/*          EXEC          */
inline std::string getFilename(std::string path) { return path.find_last_of("/\\") == std::string::npos ? path : path.substr(path.find_last_of("/\\") + 1); }
std::filesystem::path getProgramPath();
inline uint64_t lastModified(const std::string& filename) {
  if (!std::filesystem::exists(filename)) return 0;
  return std::filesystem::last_write_time(filename).time_since_epoch().count();
}

bool execute(std::string command);

/*          ERRORS          */
template <typename... Args> void error(FILE* file, Args... args) {
  if (file) {
    uint32_t pos = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint16_t line = 1;
    for (int i = 0; i < pos; i++) {
      if (getc(file) == '\n') line++;
    }
    fprintf(stderr, "Error at around file:%u: ", line);
  }
  fprintf(stderr, args...);
  fflush(stderr);
  exit(-1);
}

template <typename... Args> inline void error(Args... args) { error<Args...>(nullptr, args...); }
