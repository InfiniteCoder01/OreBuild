#pragma once
#include <filesystem>
#include <string>
#include <regex>

/*          WILDCARD          */
std::string replace(std::string str, const std::string& from, const std::string& to) {
  if (from.empty()) return str;
  size_t start = 0;
  while ((start = str.find(from, start)) != std::string::npos) {
    str.replace(start, from.length(), to);
    start += to.length();
  }
  return str;
}

bool wildcardMatch(const std::string& str, const std::string& pattern) {
  std::regex specialChars{R"([-[\]{}()+.,\^$|#])"};
  return std::regex_match(str, std::regex(replace(replace(replace(std::string(std::regex_replace(pattern, specialChars, R"(\$&)")), "?", "."), "**", ".+"), "*", "[^/]+")));
}

std::vector<std::string> wildcard(const std::string& pattern, bool dir = false) {
  std::vector<std::string> result;
  size_t wildcardStart = pattern.find_first_of("*?");
  if (wildcardStart == std::string::npos) return std::vector<std::string>{pattern};
  std::string parent = pattern.substr(0, pattern.find_last_of("/\\", wildcardStart));
  if (parent.length() == pattern.length()) parent = ".";
  for (std::filesystem::recursive_directory_iterator i(parent), end; i != end; i++) {
    if (is_directory(i->path()) == dir) {
      std::string path = i->path().string();
      replace(path.begin(), path.end(), '\\', '/');
      if (path.substr(0, 2) == "./") path = path.substr(2);
      if (wildcardMatch(path, pattern)) result.push_back(path);
    }
  }
  return result;
}

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
