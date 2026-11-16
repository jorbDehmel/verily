#include "packages.hpp"
#include "parse.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

const static std::string info_file_name = "verily_package.txt";

/// Ensures that a package has a well-formatted info file, then
/// returns a 2-tuple containing (name, version). The argument
/// should be the root of the package. The info file should be
/// a 1 (or 2 if you pad with a newline) line text file that
/// contains a string of form 'name-version'. For example,
/// `std-0.1.0` would be valid.
std::pair<std::string, PackageVersion>
get_package_info(const std::filesystem::path &_path) {
  if (!std::filesystem::exists(_path)) {
  } else if (!std::filesystem::exists(_path / info_file_name)) {
  }
  const auto contents = load_file(_path / info_file_name);
  const auto pos_of_first_dash = contents.find('-');
  if (pos_of_first_dash == contents.npos) {
    return {contents, PackageVersion()};
  }
  const auto name = contents.substr(0, pos_of_first_dash);
  std::string package_str =
      contents.substr(pos_of_first_dash + 1);
  while (!package_str.empty() && package_str.ends_with('\n')) {
    package_str.pop_back();
  }
  return {name, PackageVersion::from_str(package_str)};
}

/// Clone to the OS's temp folder and return the root of the
/// cloned thing.
std::filesystem::path git_clone(const std::string &_url) {
  const auto cur_path = std::filesystem::current_path();
  std::filesystem::current_path(
      std::filesystem::temp_directory_path());

  const auto res = std::system(
      ("git clone " + _url + " verily_package_tmp").c_str());
  const auto out_path = std::filesystem::temp_directory_path() /
                        "verily_package_tmp";

  std::filesystem::current_path(cur_path);
  if (res != 0) {
    throw std::runtime_error("Failed git clone of url '" +
                             _url + "'");
  }
  return out_path;
}

std::strong_ordering PackageVersion::operator<=>(
    const PackageVersion &_other) const noexcept {
  std::list<size_t> my_version = version;
  std::list<size_t> other_version = _other.version;
  while (my_version.size() < other_version.size()) {
    my_version.push_back(0);
  }
  while (other_version.size() < my_version.size()) {
    other_version.push_back(0);
  }
  auto a = my_version.begin();
  auto b = other_version.begin();
  while (a != my_version.end() && b != other_version.end()) {
    if (*a != *b) {
      if (*a > *b) {
        return std::strong_ordering::greater;
      } else {
        return std::strong_ordering::less;
      }
    }
    ++a;
    ++b;
  }
  return std::strong_ordering::equal;
}

PackageVersion::operator std::string() const noexcept {
  if (version.empty()) {
    return "0";
  }
  std::string out;
  bool first = true;
  for (const auto &v : version) {
    if (first) {
      first = false;
    } else {
      out += ".";
    }
    out += std::to_string(v);
  }
  return out;
}

PackageVersion::operator std::filesystem::path()
    const noexcept {
  if (version.empty()) {
    return "0";
  }
  std::string out;
  bool first = true;
  for (const auto &v : version) {
    if (first) {
      first = false;
    } else {
      out += "_";
    }
    out += std::to_string(v);
  }
  return out;
}

PackageVersion
PackageVersion::from_str(const std::string &_from) {
  std::list<size_t> out;
  std::string cur;
  for (const char &c : _from + ".") {
    if (c < '0' || '9' < c) {
      if (!cur.empty()) {
        try {
          out.push_back(std::stoull(cur));
        } catch (...) {
          throw std::runtime_error("Invalid version segment '" +
                                   cur + "' in " + _from);
        }
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  return PackageVersion(out);
}

PackageVersion PackageVersion::next() const noexcept {
  std::list<size_t> next_version = version;
  next_version.push_back(1);
  return PackageVersion(next_version);
}

std::ostream &operator<<(std::ostream &_strm,
                         const PackageVersion &_version) {
  return _strm << std::string(_version);
}

std::filesystem::path
PackageManager::find(const std::filesystem::path &_path,
                     const std::filesystem::path &_cur_path) {
  if (!find_cache.contains({_path, _cur_path})) {
    // Local match
    if (std::filesystem::exists(_cur_path.parent_path() /
                                _path)) {
      find_cache[{_path, _cur_path}] =
          std::filesystem::canonical(_cur_path.parent_path() /
                                     _path);
    }

    // Package exact match
    else if (std::filesystem::exists(packages_path / _path)) {
      find_cache[{_path, _cur_path}] =
          std::filesystem::canonical(packages_path / _path);
    } else if (std::filesystem::exists(packages_path / version /
                                       _path)) {
      find_cache[{_path, _cur_path}] =
          std::filesystem::canonical(packages_path / version /
                                     _path);
    }

    // General package search
    else {
      const auto pos_of_first_slash = _path.string().find('/');
      const std::string package_name =
          _path.string().substr(0, pos_of_first_slash);

      if (!has(package_name)) {
        throw std::runtime_error("Package '" + package_name +
                                 "' is not present");
      }

      const std::filesystem::path subpath =
          _path.string().substr(pos_of_first_slash + 1);

      std::optional<PackageVersion> lowest_allowed;
      std::optional<PackageVersion> lowest_too_high;
      if (constraints.contains(package_name)) {
        lowest_allowed = constraints.at(package_name).first;
        lowest_too_high = constraints.at(package_name).second;
      }

      // Find highest compliant version
      PackageVersion v;
      bool found = false;
      for (const auto &package_version :
           all_versions(package_name)) {
        if (package_version >= v) {
          if (!lowest_allowed.has_value() ||
              lowest_allowed.value() <= package_version) {
            if (!lowest_too_high.has_value() ||
                lowest_too_high.value() > package_version) {
              found = true;
              v = package_version;
            }
          }
        }
      }

      if (!found) {
        std::string bounds;
        if (lowest_allowed.has_value()) {
          bounds +=
              std::string(lowest_allowed.value()) + " <= ";
        }
        bounds += package_name;
        if (lowest_too_high.has_value()) {
          bounds +=
              " < " + std::string(lowest_too_high.value());
        }

        throw std::runtime_error(
            "Failed to find package satisfying constraint: " +
            bounds);
      }

      // Actually localize
      find_cache[{_path, _cur_path}] =
          std::filesystem::canonical(packages_path / version /
                                     package_name / v /
                                     subpath);
    }
  }
  return find_cache.at({_path, _cur_path});
}

void PackageManager::add_package(
    const std::string &_git_repo_url) {
  const auto cur_path = std::filesystem::current_path();
  const auto local_path = git_clone(_git_repo_url);
  return add_local_package(local_path);
}

void PackageManager::add_local_package(
    const std::filesystem::path &_path) {
  const auto cur_path = std::filesystem::current_path();
  const auto actual_path = std::filesystem::canonical(
      std::filesystem::current_path() / _path);

  const auto info = get_package_info(actual_path);
  const auto name = info.first;
  const auto package_version = info.second;

  if (!std::filesystem::exists(packages_path)) {
    std::filesystem::create_directory(packages_path);
  }
  std::filesystem::current_path(packages_path);
  if (!std::filesystem::exists(version)) {
    std::filesystem::create_directory(version);
  }
  std::filesystem::current_path(version);

  if (!std::filesystem::exists(name)) {
    std::filesystem::create_directory(name);
  }
  std::filesystem::current_path(name);

  std::filesystem::copy(
      actual_path, package_version,
      std::filesystem::copy_options::recursive);

  std::filesystem::current_path(cur_path);
}

void PackageManager::remove_package(
    const std::string &_name, const PackageVersion &_version) {
  const auto path = packages_path / version / _name / _version;
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error(
        "Cannot remove package which isn't present!");
  }
  std::filesystem::remove_all(path);
  if (std::filesystem::is_empty(packages_path / version /
                                _name)) {
    std::filesystem::remove_all(packages_path / version /
                                _name);
    if (std::filesystem::is_empty(packages_path / version)) {
      std::filesystem::remove_all(packages_path / version);
      if (std::filesystem::is_empty(packages_path)) {
        std::filesystem::remove(packages_path);
      }
    }
  }
}

bool PackageManager::has(
    const std::string &_name) const noexcept {
  const auto path = packages_path / version / _name;
  return std::filesystem::exists(path) &&
         std::filesystem::is_directory(path);
}

bool PackageManager::has(
    const std::string &_name,
    const PackageVersion &_version) const noexcept {
  if (!has(_name)) {
    return false;
  }
  const auto path = packages_path / version / _name / _version;
  return std::filesystem::exists(path) &&
         std::filesystem::is_directory(path);
}

std::list<PackageVersion> PackageManager::all_versions(
    const std::string &_name) noexcept {
  if (!all_versions_cache.contains(_name)) {
    std::list<PackageVersion> out;
    for (const auto &fp : std::filesystem::directory_iterator(
             packages_path / version / _name)) {
      if (std::filesystem::is_directory(fp)) {
        const std::string version = fp.path().filename();
        out.push_back(PackageVersion::from_str(version));
      }
    }
    all_versions_cache[_name] = out;
  }
  return all_versions_cache.at(_name);
}

void PackageManager::restrict_version_lt(
    const std::string &_name, const PackageVersion &_version) {
  if (_name == "verily") {
    if (version >= _version) {
      throw std::runtime_error(
          "Required verily version to be < " +
          std::string(_version) + ", but is " +
          std::string(version));
    }
    return;
  }

  if (constraints.contains(_name)) {
    if (!constraints.at(_name).second.has_value() ||
        _version < constraints.at(_name).second.value()) {
      constraints[_name].second = _version;
    }
  } else {
    constraints[_name] = {{}, _version};
  }
}

void PackageManager::restrict_version_ge(
    const std::string &_name, const PackageVersion &_version) {
  if (_name == "verily") {
    if (version < _version) {
      throw std::runtime_error(
          "Required verily version to be >= " +
          std::string(_version) + ", but is " +
          std::string(version));
    }
    return;
  }

  if (constraints.contains(_name)) {
    if (!constraints.at(_name).first.has_value() ||
        _version > constraints.at(_name).first.value()) {
      constraints[_name].first = _version;
    }
  } else {
    constraints[_name] = {_version, {}};
  }
}

void PackageManager::remove_verily_version(
    const PackageVersion &_version) const noexcept {
  const auto path = packages_path / _version;
  if (!std::filesystem::exists(path)) {
    return;
  }
  std::filesystem::remove_all(path);
  if (std::filesystem::is_empty(packages_path / _version)) {
    std::filesystem::remove_all(packages_path / _version);
    if (std::filesystem::is_empty(packages_path)) {
      std::filesystem::remove(packages_path);
    }
  }
}

void PackageManager::set_verily_version(
    const PackageVersion &_version) const {
  // Assert we have that version
  const auto all = all_verily_versions();
  bool seen = false;
  for (const PackageVersion &v : all) {
    if (v <= _version && v >= _version) { // who knows
      if (!std::filesystem::exists(packages_path / v /
                                   "verily.out")) {
        throw std::runtime_error(
            "Version '" + std::string(v) +
            "' does not have verily executable");
      }
      if (!std::filesystem::exists(packages_path / v /
                                   "hark.out")) {
        throw std::runtime_error(
            "Version '" + std::string(v) +
            "' does not have hark executable");
      }
      seen = true;
      break;
    }
  }
  if (!seen) {
    throw std::runtime_error(
        "Cannot set verily version to non-present " +
        std::string(_version));
  }

  // Remove old symlinks
  std::filesystem::remove(executables_path / "verily");
  std::filesystem::remove(executables_path / "hark");

  // Add new symlinks
  std::filesystem::create_symlink(packages_path / _version /
                                      "verily.out",
                                  executables_path / "verily");
  std::filesystem::create_symlink(packages_path / _version /
                                      "hark.out",
                                  executables_path / "hark");
}

std::list<PackageVersion>
PackageManager::all_verily_versions() const noexcept {
  std::list<PackageVersion> out;
  if (!std::filesystem::exists(packages_path)) {
    return out;
  }
  for (const auto &fp :
       std::filesystem::directory_iterator(packages_path)) {
    if (std::filesystem::is_directory(fp)) {
      const std::string version = fp.path().filename();
      out.push_back(PackageVersion::from_str(version));
    }
  }
  return out;
}

void PackageManager::add_verily_version(
    const std::string &_url) const {
  const auto verily_path = git_clone(_url);
  add_local_verily_version(verily_path);
}

void PackageManager::add_local_verily_version(
    const std::filesystem::path &_path) const {
  const auto actual_path = std::filesystem::canonical(
      std::filesystem::current_path() / _path);

  if (!std::filesystem::exists(actual_path / "verily.out")) {
    throw std::runtime_error(
        "Cannot add local verily version where verily "
        "executable is missing");
  } else if (!std::filesystem::exists(actual_path /
                                      "hark.out")) {
    throw std::runtime_error(
        "Cannot add local verily version where hark "
        "executable is missing");
  }

  const auto pipe = popen(
      ((actual_path / "verily.out").string() + " --version")
          .c_str(),
      "r");
  char buffer[32];
  std::string result;
  try {
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
      result += buffer;
    }
  } catch (...) {
    pclose(pipe);
    throw;
  }
  pclose(pipe);
  const auto verily_version = PackageVersion::from_str(result);

  const auto cur_path = std::filesystem::current_path();

  if (!std::filesystem::exists(packages_path)) {
    std::filesystem::create_directory(packages_path);
  }
  std::filesystem::current_path(packages_path);

  if (!std::filesystem::exists(verily_version)) {
    std::filesystem::create_directory(verily_version);
  }

  std::filesystem::copy(actual_path / "verily.out",
                        packages_path / verily_version /
                            "verily.out");
  std::filesystem::copy(actual_path / "hark.out",
                        packages_path / verily_version /
                            "hark.out");

  std::filesystem::current_path(cur_path);
}
