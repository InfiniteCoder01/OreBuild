#include <set>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdio.h>
#include "common.hpp"
#include "package.hpp"

#include <sys/types.h>
#include <sys/stat.h>

// Windows: g++ -std=c++17 src\main.cpp -o bin\OreBuild.exe
// Linux: g++ -std=c++17 src/main.cpp -o bin/OreBuild

/*          FILES          */
int fpeek(FILE* file) {
  int c = fgetc(file);
  ungetc(c, file);
  return c;
}

inline void sskip(FILE* file) {
  while (isspace(fpeek(file))) fgetc(file);
}

uint64_t lastModified(const std::string& filename) {
  struct stat result;
  if (stat(filename.c_str(), &result) != 0) {
    return 0;
  }
  return result.st_mtime;
}

/*          UTILS          */
std::string getFilename(std::string path) { return path.find_last_of("/\\") == std::string::npos ? path : path.substr(path.find_last_of("/\\") + 1); }
bool execute(std::string command) {
  if (command[0] == '@') command.erase(command.begin());
  else puts(command.c_str());
  return system(command.c_str()) == 0;
}

/*          MAIN          */
std::set<std::string> objects;
std::vector<std::string> buildModule(const std::string& buildfile) {
  std::unordered_map<std::string, std::vector<std::string>> properties;
  const std::vector<std::string> props = {"library", "include", "files", "watch", "output", "flags"};
  const std::vector<std::string> multiples = {"library", "include", "files", "watch", "flags"};
  const auto path = std::filesystem::current_path();
  const std::string filename = getFilename(buildfile);
  std::string icaseFilename(filename.length(), ' ');
  transform(filename.begin(), filename.end(), icaseFilename.begin(), ::tolower);
  const bool buildLibrary = icaseFilename == "library.orebuild";

  if (!std::filesystem::exists(buildfile)) error("Buildfile '%s' not found!", buildfile.c_str());
  if (buildfile.find_last_of("/\\") != std::string::npos) std::filesystem::current_path(buildfile.substr(0, buildfile.find_last_of("/\\")));

  FILE* file = fopen(filename.c_str(), "r");
  while (true) {
    sskip(file);
    if (feof(file)) break;
    if (fpeek(file) == '/') {
      if (fgetc(file), fpeek(file) == '/') {
        while (!feof(file) && fgetc(file) != '\n') true;
        continue;
      } else ungetc('/', file);
    }

    std::string property(1, getc(file));
    if (!isalpha(property[0])) error(file, "Expected property name!\n");
    while (isalpha(fpeek(file))) property += fgetc(file);
    if (!std::count(props.begin(), props.end(), property)) error(file, "Invalid property: %s!\n", property.c_str());
    bool multiple = std::count(multiples.begin(), multiples.end(), property);

    sskip(file);
    if (fgetc(file) != '"') error(file, "Expected property value as double-quoted string!\n");
    std::vector<std::string> values = {""};
    char c;
    while (!feof(file) && (c = fgetc(file)) != '"') {
      if (multiple && c == ' ') {
        values.push_back("");
        sskip(file);
        continue;
      }
      values[values.size() - 1] += c;
    }
    if (c != '"') error(file, "Unterminated string!");
    sskip(file);
    if (fgetc(file) != ';') error(file, "Missing semicolon!");

    if (!properties.count(property)) properties[property] = values;
    else if (multiple) properties[property].insert(properties[property].end(), values.begin(), values.end());
    else error(file, "Repeating property: %s!", property.c_str());
  }
  fclose(file);

  if (!properties.count("library")) properties["library"] = {};
  if (!properties.count("include")) properties["include"] = (buildLibrary ? std::vector<std::string>{"."} : std::vector<std::string>{});
  if (!properties.count("files")) properties["files"] = {"src/**.cpp"};
  if (!properties.count("flags")) properties["flags"] = {};

  std::vector<std::string> files, watch, includes;
  for (const auto& pattern : properties["files"]) {
    std::vector<std::string> filesFound = wildcard(pattern);
    files.insert(files.end(), filesFound.begin(), filesFound.end());
  }
  for (const auto& pattern : properties["watch"]) {
    std::vector<std::string> watchFound = wildcard(pattern);
    watch.insert(watch.end(), watchFound.begin(), watchFound.end());
  }
  for (const auto& pattern : properties["include"]) {
    std::vector<std::string> includesFound = wildcard(pattern);
    includes.insert(includes.end(), includesFound.begin(), includesFound.end());
  }

  if (!buildLibrary) objects.clear();
  for (const auto& library : properties["library"]) {
    auto path = libdirPath / library / "library.orebuild";
    if(!std::filesystem::exists(path)) continue;
    std::vector<std::string> newIncludes = buildModule(path.string());
    includes.insert(includes.begin(), newIncludes.begin(), newIncludes.end());
    for (const auto& object : std::filesystem::directory_iterator(libdirPath / library / "build")) {
      objects.insert(std::filesystem::absolute(object.path()).string());
    }
  }

  if (buildLibrary) {
    if (properties.count("output")) error("Library output specified!");
    if (!std::filesystem::exists("build")) std::filesystem::create_directory("build");

    bool skip = true;
    watch.insert(watch.end(), files.begin(), files.end());
    for (const auto& file : watch) {
      if (lastModified(file) > lastModified("build/" + getFilename(files[0]) + ".o")) {
        skip = false;
        break;
      }
    }

    if (!skip) {
      for (const auto& file : files) {
        std::string command = (file.substr(file.size() - 2) == ".c" ? "gcc -c " : "g++ -c ");
        command += file + ' ';
        command += "-o build/" + getFilename(file) + ".o" + ' ';
        for (const auto& include : includes) command += "-I" + include + ' ';
        for (const auto& flag : properties["flags"]) command += flag + ' ';
        if (!execute(command.c_str())) exit(-1);
      }
    }

    for (auto& include : includes) include = std::filesystem::absolute(include).string();
    std::filesystem::current_path(path);
    return includes;
  } else {
    if (!properties.count("output")) error("No output specified!");
    files.insert(files.begin(), objects.begin(), objects.end());
    std::string command = "@g++ ";

    bool skip = true;
    watch.insert(watch.end(), files.begin(), files.end());
    for (const auto& file : watch) {
      if (lastModified(file) > lastModified(properties["output"][0])) {
        skip = false;
        break;
      }
    }

    if (!skip) {
      for (const auto& file : files) command += file + ' ';
      command += "-o " + properties["output"][0] + ' ';
      for (const auto& include : includes) command += "-I" + include + ' ';
      for (const auto& flag : properties["flags"]) command += flag + ' ';
      if (!execute(command.c_str())) exit(-1);
    }

    std::filesystem::current_path(path);
    return properties["output"];
  }
}

int main(int argc, char** argv) {
  char buffer[1024];
  #if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
  GetModuleFileName(nullptr, buffer, sizeof(buffer));
  #else
  readlink("/proc/self/exe", buffer, sizeof(buffer));
  #endif
  libdirPath = std::filesystem::absolute(std::filesystem::path(buffer).parent_path() / "libraries");
  if (argc == 2) {
    if (strcmp(argv[1], "build") == 0) buildModule("project.orebuild");
    else if (strcmp(argv[1], "run") == 0) return !execute(buildModule("project.orebuild")[0]) * -1;
    else error("Unknown cmd option: %s!\n", argv[1]);
  } else if (argc == 3) {
    if (strcmp(argv[1], "search") == 0) searchPackage(argv[2]);
    else if (strcmp(argv[1], "install") == 0) installPackage(argv[2]);
    else error("Unknown cmd option: %s!\n", argv[1]);
  } else error("Usage: OreBuild (build/run/search/install) [githubPackageID]!\n");
  return 0;
}
