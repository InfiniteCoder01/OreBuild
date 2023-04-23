#include "00Names.hpp"
#include <set>

inline int fpeek(FILE* file) {
  const int c = fgetc(file);
  ungetc(c, file);
  return c;
}

inline void sskip(FILE* file) {
  while (isspace(fpeek(file))) fgetc(file);
}

inline bool match(FILE* file, char character) {
  const int c = fgetc(file);
  if (c == character) return true;
  ungetc(c, file);
  return false;
}

static std::string readWord(FILE* file, const std::string& message = "Expected name!\n") {
  std::string word(1, getc(file));
  if (!isalpha(word[0])) error(file, message.c_str());
  while (isalpha(fpeek(file))) word += (char)fgetc(file);
  return word;
}

static std::vector<std::string> readSplitString(FILE* file, bool split) {
  std::vector<std::string> values = {""};
  char c;
  while (!feof(file) && (c = fgetc(file)) != '"') {
    if (split && c == ' ') {
      values.emplace_back();
      sskip(file);
      continue;
    }
    *values.rbegin() += c;
  }
  if (c != '"') error(file, "Unterminated string!\n");
  return values;
}

std::unordered_map<std::string, std::vector<std::string>> parseFile(const std::string& filename) {
  std::unordered_map<std::string, std::vector<std::string>> properties;
  const std::set<std::string> props = {"library", "include", "files", "watch", "output", "flags", "linkerFlags", "compiler"};
  const std::set<std::string> multiples = {"library", "include", "files", "watch", "flags", "linkerFlags"};

  FILE* file = fopen(filename.c_str(), "r");
  bool ignore = false;
  while (true) {
    sskip(file);
    if (feof(file)) break;
    if (fpeek(file) == '/') {
      if (fgetc(file), fpeek(file) == '/') {
        while (!feof(file) && fgetc(file) != '\n') continue;
        continue;
      }
      ungetc('/', file);
    }

    // * Conditions
    bool resetIgnore = false;
    if (fpeek(file) == '[') {
      getc(file);
      const bool expect = !match(file, '!');
      sskip(file);
      std::string condition(1, getc(file));
      if (condition[0] != '*') {
        if (!isalpha(condition[0])) error(file, "Expected configuration or platform name!\n");
        while (isalpha(fpeek(file))) condition += (char)fgetc(file);
      }
      if (fgetc(file) != ']') error(file, "Expected ']'!\n");

      const bool value = (platform == condition) || (configuration == condition) || condition == "*";
      if (!match(file, ':') && !ignore) {
        resetIgnore = true;
        ignore = value != expect;
      } else {
        ignore = value != expect;
        continue;
      }
      sskip(file);
    }

    // * Properties
    const auto property = readWord(file, "Expected property name!\n");
    if (!props.count(property)) error(file, "Invalid property: %s!\n", property.c_str());
    const bool multiple = multiples.count(property);

    sskip(file);
    if (fgetc(file) != '"') error(file, "Expected property value as double-quoted string!\n");
    std::vector<std::string> values = readSplitString(file, multiple);
    sskip(file);
    if (fgetc(file) != ';') error(file, "Missing semicolon!\n");

    if (!ignore) {
      if (!properties.count(property)) properties[property] = values;
      else if (multiple) properties[property].insert(properties[property].end(), values.begin(), values.end());
      else error(file, "Repeating property: %s!\n", property.c_str());
    }
    if (resetIgnore) ignore = false;
  }
  fclose(file);
  return properties;
}
