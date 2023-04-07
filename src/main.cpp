#include "00Names.hpp"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string.h>
#include <string>
#include <unordered_map>

// Windows: g++ -std=c++17 -Ofast src\*.cpp -o bin\OreBuild.exe
// Linux: g++ -std=c++17 -Ofast src/*.cpp -o bin/OreBuild

std::filesystem::path libdirPath;

/*          FILES          */
int fpeek(FILE* file) {
  const int c = fgetc(file);
  ungetc(c, file);
  return c;
}

inline void sskip(FILE* file) {
  while (isspace(fpeek(file))) fgetc(file);
}

/*          MAIN          */
std::set<std::string> objects;
std::vector<std::string> linkerFlags;
bool relink, skip;

std::vector<std::string> buildModule(const std::filesystem::path& buildfile, bool& skip) {
  // * Variables & Constants
  std::unordered_map<std::string, std::vector<std::string>> properties;

  const std::set<std::string> props = {"library", "include", "files", "watch", "output", "flags", "linkerFlags", "compiler"};
  const std::set<std::string> multiples = {"library", "include", "files", "watch", "flags", "linkerFlags"};

  // * Files & Names
  if (!std::filesystem::exists(buildfile)) error("Buildfile '%s' not found!", buildfile.c_str());
  const auto originalPath = std::filesystem::current_path();
  std::filesystem::current_path(buildfile.parent_path());

  const std::string filename = buildfile.filename().string();
  std::string icaseFilename(filename.length(), ' ');
  std::transform(filename.begin(), filename.end(), icaseFilename.begin(), ::tolower);
  const bool link = icaseFilename == "project.orebuild";

  // * Parse build file
  FILE* file = fopen(filename.c_str(), "r");
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

    std::string property(1, getc(file));
    if (!isalpha(property[0])) error(file, "Expected property name!\n");
    while (isalpha(fpeek(file))) property += (char)fgetc(file);
    if (!props.count(property)) error(file, "Invalid property: %s!\n", property.c_str());
    const bool multiple = multiples.count(property);

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
      *values.rbegin() += c;
    }
    if (c != '"') error(file, "Unterminated string!");
    sskip(file);
    if (fgetc(file) != ';') error(file, "Missing semicolon!");

    if (!properties.count(property)) properties[property] = values;
    else if (multiple) properties[property].insert(properties[property].end(), values.begin(), values.end());
    else error(file, "Repeating property: %s!", property.c_str());
  }
  fclose(file);

  // * Add missing keys
  if (!properties.count("library")) properties["library"] = {};
  if (!properties.count("include")) properties["include"] = {"."};
  if (!properties.count("files")) properties["files"] = {"src/**.cpp"};
  if (!properties.count("watch")) properties["watch"] = {"src/**.h", "src/**.hpp"};
  if (!properties.count("flags")) properties["flags"] = {};

  // * Apply wildcards and gather files
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

  // * Dependencies
  if (link) {
    objects.clear();
    linkerFlags.clear();
    relink = false;
  }

  if (properties.count("linkerFlags")) linkerFlags.insert(linkerFlags.end(), properties["linkerFlags"].begin(), properties["linkerFlags"].end());

  std::vector<std::string> localIncludes;
  skip = true;
  for (const auto& library : properties["library"]) {
    auto path = libdirPath / library / "library.orebuild";
    if (!std::filesystem::exists(path)) continue; // TODO: optional dependencies

    bool skipModule = true;
    std::vector<std::string> newIncludes = buildModule(std::filesystem::absolute(path), skipModule);
    if (!skipModule) skip = false;
    localIncludes.insert(localIncludes.end(), newIncludes.begin(), newIncludes.end());
    for (const auto& object : std::filesystem::directory_iterator(libdirPath / library / "build")) {
      objects.insert(std::filesystem::absolute(object.path()).string());
    }
  }

  // * Get the compile commands
  const std::string compiler = properties.count("compiler") ? properties["compiler"][0] : "gcc";
  std::string cxxCompiler = compiler;
  if (compiler.substr(compiler.size() - 2) == "cc") std::replace(cxxCompiler.end() - 2, cxxCompiler.end(), 'c', '+');
  else cxxCompiler += "++";

  // * Check for skips
  if (!std::filesystem::exists("build")) {
    std::filesystem::create_directory("build");
    skip = false;
  }

  if (skip) {
    for (const auto& file : watch) {
      if (lastModified(file) >= lastModified("build")) {
        skip = false;
        break;
      }
    }
  }

  // * Recompile some objects
  for (const auto& file : files) {
    if (skip && lastModified(file) < lastModified("build/" + getFilename(files[0]) + ".o")) continue;
    std::string command = compiler + " -c ";
    if (file.substr(file.size() - 4) == ".cpp") command = cxxCompiler + " -c ";
    command += file + ' ';
    command += "-o build/" + getFilename(file) + ".o" + ' ';
    for (const auto& include : includes) command += "-I" + include + ' ';
    for (const auto& include : localIncludes) command += "-I" + include + ' ';
    for (const auto& flag : properties["flags"]) command += flag + ' ';
    if (!execute(command)) exit(-1);
    relink = true;
  }

  if (!link) {
    if (properties.count("output")) puts("Warning: Library output specified!");
    for (auto& include : includes) include = std::filesystem::absolute(include).string();
    std::filesystem::current_path(originalPath);
    return includes;
  }

  for (const auto& object : std::filesystem::directory_iterator(std::filesystem::path(buildfile).parent_path() / "build")) {
    objects.insert(std::filesystem::absolute(object.path()).string());
  }

  if (!properties.count("output")) error("No output specified!");
  if (relink) {
    std::string command = "@" + cxxCompiler + " ";
    std::replace(command.begin(), command.end(), 'c', '+');

    for (const auto& object : objects) command += object + ' ';
    command += "-o " + properties["output"][0] + ' ';

    for (const auto& flag : linkerFlags) command += flag + ' ';
    if (!execute(command)) exit(-1);
  }

  std::filesystem::current_path(originalPath);
  return properties["output"];
}

int main(int argc, char** argv) { // TODO: better command line argument parser
  libdirPath = std::filesystem::absolute(getProgramPath().parent_path() / "libraries");
  if (argc == 2) {
    bool skip = true;
    if (strcmp(argv[1], "build") == 0) buildModule(std::filesystem::absolute("project.orebuild"), skip);
    else if (strcmp(argv[1], "run") == 0) return !execute(buildModule(std::filesystem::absolute("project.orebuild"), skip)[0]) * -1;
    else error("Unknown cmd option: %s!\n", argv[1]);
  } else if (argc == 3) {
    if (strcmp(argv[1], "search") == 0) searchPackage(argv[2]);
    else if (strcmp(argv[1], "install") == 0) installPackage(argv[2]);
    else error("Unknown cmd option: %s!\n", argv[1]);
  } else error("Usage: OreBuild (build/run/search/install) [githubPackageID]!\n");
  return 0;
}
