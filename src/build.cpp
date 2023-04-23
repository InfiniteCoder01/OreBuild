#include "00Names.hpp"
#include <set>
#include <algorithm>

static std::set<std::string> objects;
static std::vector<std::string> linkerFlags;
static bool relink, skip;
bool rebuild = false;

std::vector<std::string> buildModule(const std::filesystem::path& buildfile, bool& skip) {
  // * Files & Names
  if (!std::filesystem::exists(buildfile)) error("Buildfile '%s' not found!", buildfile.c_str());
  const auto originalPath = std::filesystem::current_path();
  std::filesystem::current_path(buildfile.parent_path());

  const auto filename = buildfile.filename().string();
  std::string icaseFilename(filename.length(), ' ');
  std::transform(filename.begin(), filename.end(), icaseFilename.begin(), ::tolower);
  const bool link = icaseFilename == "project.orebuild";

  auto properties = parseFile(filename);

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
  skip = !rebuild;
  for (const auto& library : properties["library"]) {
    auto path = libdirPath / library / "library.orebuild";
    if (!std::filesystem::exists(path)) continue; // TODO: optional dependencies

    bool skipModule = true;
    std::vector<std::string> newIncludes = buildModule(std::filesystem::absolute(path), skipModule);
    if (!skipModule) skip = false;
    localIncludes.insert(localIncludes.end(), newIncludes.begin(), newIncludes.end());
    for (const auto& object : std::filesystem::directory_iterator(libdirPath / library / "build" / platform / configuration)) {
      objects.insert(std::filesystem::absolute(object.path()).string());
    }
  }

  // * Get the compile commands
  const std::string compiler = properties.count("compiler") ? properties["compiler"][0] : "gcc";
  std::string cxxCompiler = compiler;
  if (compiler.substr(compiler.size() - 2) == "cc") std::replace(cxxCompiler.end() - 2, cxxCompiler.end(), 'c', '+');
  else cxxCompiler += "++";

  // * Check for skips
  if (!std::filesystem::exists(std::filesystem::path("build") / platform / configuration)) {
    std::filesystem::create_directories(std::filesystem::path("build") / platform / configuration);
    skip = false;
  }

  if (skip) {
    for (const auto& file : watch) {
      if (lastModified(file) >= lastModified(std::filesystem::path("build") / platform / configuration)) {
        skip = false;
        break;
      }
    }
  }

  // * Recompile some objects
  for (const auto& file : files) {
    if (skip && lastModified(file) < lastModified(std::filesystem::path("build") / platform / configuration / (getFilename(files[0]) + ".o"))) continue;
    std::string command = compiler + " -c ";
    if (file.substr(file.size() - 4) == ".cpp") command = cxxCompiler + " -c ";
    command += file + ' ';
    command += "-o build/";
    command += platform + '/';
    command += configuration + '/';
    command += getFilename(file) + ".o" + ' ';
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

  for (const auto& object : std::filesystem::directory_iterator(std::filesystem::path(buildfile).parent_path() / "build" / platform / configuration)) {
    objects.insert(std::filesystem::absolute(object.path()).string());
  }

  if (!properties.count("output")) error("No output specified!");
  if (!std::filesystem::exists(properties["output"][0])) {
    std::filesystem::create_directories(std::filesystem::path(properties["output"][0]).parent_path());
    relink = true;
  }
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
