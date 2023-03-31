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
  int c = fgetc(file);
  ungetc(c, file);
  return c;
}

inline void sskip(FILE* file) {
  while (isspace(fpeek(file))) fgetc(file);
}

/*          MAIN          */
std::set<std::string> objects;
std::vector<std::string> linkerFlags;

std::vector<std::string> buildModule(const std::filesystem::path& buildfile) {
  std::unordered_map<std::string, std::vector<std::string>> properties;

  const std::set<std::string> props = {"library", "include", "files", "watch", "output", "flags", "linkerFlags", "compiler"};
  const std::set<std::string> multiples = {"library", "include", "files", "watch", "flags", "linkerFlags"};

  if (!std::filesystem::exists(buildfile)) error("Buildfile '%s' not found!", buildfile.c_str());
  const auto originalPath = std::filesystem::current_path();
  std::filesystem::current_path(buildfile.parent_path());

  const std::string filename = buildfile.filename();
  std::string icaseFilename(filename.length(), ' ');
  std::transform(filename.begin(), filename.end(), icaseFilename.begin(), ::tolower);
  const bool link = icaseFilename == "project.orebuild";

  FILE* file = fopen(filename.c_str(), "r");
  while (true) {
    sskip(file);
    if (feof(file)) break;
    if (fpeek(file) == '/') {
      if (fgetc(file), fpeek(file) == '/') {
        while (!feof(file) && fgetc(file) != '\n') continue;
        continue;
      } else ungetc('/', file);
    }

    std::string property(1, getc(file));
    if (!isalpha(property[0])) error(file, "Expected property name!\n");
    while (isalpha(fpeek(file))) property += fgetc(file);
    if (!props.count(property)) error(file, "Invalid property: %s!\n", property.c_str());
    bool multiple = multiples.count(property);

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

  if (!properties.count("library")) properties["library"] = {};
  if (!properties.count("include")) properties["include"] = {"."};
  if (!properties.count("files")) properties["files"] = {"src/**.cpp"};
  if (!properties.count("watch")) properties["watch"] = {"src/**.h", "src/**.hpp"};
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

  if (properties.count("linkerFlags")) linkerFlags.insert(linkerFlags.end(), properties["linkerFlags"].begin(), properties["linkerFlags"].end());

  if (link) objects.clear(), linkerFlags.clear();
  std::vector<std::string> localIncludes;
  for (const auto& library : properties["library"]) {
    auto path = libdirPath / library / "library.orebuild";
    if (!std::filesystem::exists(path)) continue;
    std::vector<std::string> newIncludes = buildModule(std::filesystem::absolute(path));
    localIncludes.insert(localIncludes.end(), newIncludes.begin(), newIncludes.end());
    for (const auto& object : std::filesystem::directory_iterator(libdirPath / library / "build")) {
      objects.insert(std::filesystem::absolute(object.path()).string());
    }
  }

  std::string compiler = properties.count("compiler") ? properties["compiler"][0] : "gcc";
  std::string cxxCompiler = compiler;
  if (compiler.substr(compiler.size() - 2) == "cc") std::replace(cxxCompiler.end() - 2, cxxCompiler.end(), 'c', '+');
  else cxxCompiler += "++";

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
      std::string command = compiler + " -c ";
      if (file.substr(file.size() - 4) == ".cpp") command = cxxCompiler + " -c ";
      command += file + ' ';
      command += "-o build/" + getFilename(file) + ".o" + ' ';
      for (const auto& include : includes) command += "-I" + include + ' ';
      for (const auto& include : localIncludes) command += "-I" + include + ' ';
      for (const auto& flag : properties["flags"]) command += flag + ' ';
      if (!execute(command.c_str())) exit(-1);
    }
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

  if (skip) {
    for (const auto& object : objects) {
      if (lastModified(object) > lastModified(properties["output"][0])) {
        skip = false;
        break;
      }
    }
  }

  if (!properties.count("output")) error("No output specified!");
  if (!skip) {
    std::string command = "@" + cxxCompiler + " ";
    std::replace(command.begin(), command.end(), 'c', '+');

    for (const auto& object : objects) command += object + ' ';
    command += "-o " + properties["output"][0] + ' ';

    for (const auto& flag : linkerFlags) command += flag + ' ';
    if (!execute(command.c_str())) exit(-1);
  }

  std::filesystem::current_path(originalPath);
  return properties["output"];
}

int main(int argc, char** argv) { // TODO: better command line argument parser
  libdirPath = std::filesystem::absolute(getProgramPath().parent_path() / "libraries");
  if (argc == 2) {
    if (strcmp(argv[1], "build") == 0) buildModule(std::filesystem::absolute("project.orebuild"));
    else if (strcmp(argv[1], "run") == 0) return !execute(buildModule(std::filesystem::absolute("project.orebuild"))[0]) * -1;
    else error("Unknown cmd option: %s!\n", argv[1]);
  } else if (argc == 3) {
    if (strcmp(argv[1], "search") == 0) searchPackage(argv[2]);
    else if (strcmp(argv[1], "install") == 0) installPackage(argv[2]);
    else error("Unknown cmd option: %s!\n", argv[1]);
  } else error("Usage: OreBuild (build/run/search/install) [githubPackageID]!\n");
  return 0;
}
