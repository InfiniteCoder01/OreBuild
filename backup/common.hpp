#pragma once
#include <stdio.h>
#include <filesystem>
#include <vector>
#include <string>
#include <regex>
#include <map>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif

int fpeek(FILE* file) {
  int c = fgetc(file);
  ungetc(c, file);
  return c;
}

inline void skipSpaces(FILE* file) {
  while (isspace(fpeek(file))) fgetc(file);
}

void error(FILE* file, std::string error) {
  long originalPos = ftell(file);
  if (originalPos < 0) fprintf(stderr, "OreBuild:?:?: %s\n", error.c_str()), fflush(stderr), exit(-1);
  fseek(file, 0, 0);
  int c;
  int line = 1, col = 1;
  while ((ftell(file) < originalPos) && (c = fgetc(file)) > 0) {
    if (c == '\n') line++, col = 1;
    else col++;
  }
  fprintf(stderr, "OreBuild:%d:%d: %s\n", line, col, error.c_str());
  fflush(stderr);
  exit(-1);
}

inline void expect(FILE* file, char token) {
  skipSpaces(file);
  char c = fgetc(file);
  if (c != token) error(file, "Expected '" + std::string(1, token) + "', got '" + std::string(1, c) + "'!");
}

std::string readID(FILE* file) {
  skipSpaces(file);
  int beginning = fgetc(file);
  if (!isalpha(beginning) && beginning != '_') error(file, "Expected word!");
  std::string str(1, beginning);
  while (true) {
    if (!isalpha(fpeek(file)) && !isdigit(fpeek(file)) && fpeek(file) != '_') break;
    str += fgetc(file);
  }
  return str;
}

std::string readString(FILE* file) {
  skipSpaces(file);
  int beginning = fgetc(file);
  if (beginning != '\"') error(file, "Expected string!");
  std::string str;
  while (true) {
    if (fpeek(file) == '\n' || feof(file)) error(file, "Unterminated string!");
    if (fpeek(file) == '\"') break;
    str += fgetc(file);
  }
  fgetc(file);
  return str;
}

template <template <class, class, class...> class C, typename K, typename V, typename... Args> V getOrDefault(const C<K, V, Args...>& m, K const& key, const V& defval) {
  typename C<K, V, Args...>::const_iterator it = m.find(key);
  if (it == m.end()) return defval;
  return it->second;
}

std::string replace(std::string str, const std::string& from, const std::string& to) {
  if (from.empty()) return str;
  size_t start = 0;
  while ((start = str.find(from, start)) != std::string::npos) {
    str.replace(start, from.length(), to);
    start += to.length();
  }
  return str;
}

std::string escape(std::string str) {
  std::regex specialChars{R"([-[\]{}()+.,\^$|#])"};
  return std::string(std::regex_replace(str, specialChars, R"(\$&)"));
}

bool wildcardMatch(const std::string& str, const std::string& pattern) { return std::regex_match(str, std::regex(replace(replace(replace(escape(pattern), "?", "."), "*", ".*"), " ", "|"))); }

std::vector<std::string> wildcard(const std::string& pattern) {  // TODO: optimize
  std::vector<std::string> result;
  for (std::filesystem::recursive_directory_iterator i("."), end; i != end; i++) {
    if (!is_directory(i->path())) {
      std::string path = replace(i->path().string().c_str(), "\\", "/").c_str();
      if (path.substr(0, 2) == "./") path = path.substr(2);
      if (wildcardMatch(path, pattern)) {
        result.push_back(path);
      }
    }
  }
  return result;
}

std::vector<std::string> split(std::string str, char delim = ' ') {
  std::vector<std::string> result;
  size_t start = 0;
  while (start < str.length()) {
    size_t pos = str.find(delim, start);
    if (pos == std::string::npos) pos = str.length();
    result.push_back(str.substr(start, pos - start));
    start = pos + 1;
  }
  return result;
}

uint64_t lastModified(const std::string& filename) {
  struct stat result;
  if (stat(filename.c_str(), &result) != 0) {
    return 0;
  }
  return result.st_mtime;
}
