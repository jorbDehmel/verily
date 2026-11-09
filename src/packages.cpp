#include "packages.hpp"
#include <cassert>
#include <filesystem>
#include <stdexcept>

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
  bool first = true;
  for (const auto &t : _version.version) {
    if (first) {
      first = false;
    } else {
      _strm << '.';
    }
    _strm << t;
  }
  return _strm;
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
    } else {

      // General package search
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
      for (const auto &version : all_versions(package_name)) {
        if (version > v) {
          if (!lowest_allowed.has_value() ||
              lowest_allowed.value() <= version) {
            if (!lowest_too_high.has_value() ||
                lowest_too_high.value() > version) {
              v = version;
            }
          }
        }
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

bool PackageManager::add_package(
    const std::string &_name, const PackageVersion &_version,
    const std::string &_git_repo_url) {
  if (_name == "verily") {
    throw std::runtime_error(
        "Cannot add package with reserved name 'verily'");
  }
  if (has(_name, _version)) {
    return false;
  }
  const auto cur_path = std::filesystem::current_path();

  if (!std::filesystem::exists(packages_path)) {
    std::filesystem::create_directory(packages_path);
  }
  std::filesystem::current_path(packages_path);
  if (!std::filesystem::exists(version)) {
    std::filesystem::create_directory(version);
  }
  std::filesystem::current_path(version);
  if (!std::filesystem::exists(_name)) {
    std::filesystem::create_directory(_name);
  }
  std::filesystem::current_path(_name);
  const auto res = std::system(("git clone " + _git_repo_url +
                                " " + std::string(_version))
                                   .c_str());
  std::filesystem::current_path(cur_path);

  if (res != 0) {
    throw std::runtime_error("Failed to `git clone` package " +
                             _name + ", version " +
                             std::string(_version) +
                             " w/ url " + _git_repo_url);
  }

  return true;
}

bool PackageManager::add_local_package(
    const std::string &_name, const PackageVersion &_version,
    const std::filesystem::path &_path) {
  if (_name == "verily") {
    throw std::runtime_error(
        "Cannot add package with reserved name 'verily'");
  }
  if (has(_name, _version)) {
    return false;
  }

  const auto actual_path = std::filesystem::canonical(
      std::filesystem::current_path() / _path);

  const auto cur_path = std::filesystem::current_path();

  if (!std::filesystem::exists(packages_path)) {
    std::filesystem::create_directory(packages_path);
  }
  std::filesystem::current_path(packages_path);
  if (!std::filesystem::exists(version)) {
    std::filesystem::create_directory(version);
  }
  std::filesystem::current_path(version);
  if (!std::filesystem::exists(_name)) {
    std::filesystem::create_directory(_name);
  }
  std::filesystem::current_path(_name);

  std::filesystem::copy(
      actual_path, _version,
      std::filesystem::copy_options::recursive);

  std::filesystem::current_path(cur_path);
  return true;
}

bool PackageManager::remove_package(
    const std::string &_name, const PackageVersion &_version) {
  const auto path = packages_path / version / _name / _version;
  if (!std::filesystem::exists(path)) {
    return false;
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
  return true;
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

void PackageManager::add_verily_version(
    const PackageVersion &_version,
    const std::string &_url) const {
  const auto cur_path = std::filesystem::current_path();

  if (!std::filesystem::exists(packages_path)) {
    std::filesystem::create_directory(packages_path);
  }
  std::filesystem::current_path(packages_path);

  const auto res = std::system(
      ("git clone " + _url + " " + std::string(_version))
          .c_str());

  std::filesystem::current_path(cur_path);

  if (res != 0) {
    throw std::runtime_error(
        "Failed to `git clone` verily version " +
        std::string(_version) + " w/ url " + _url);
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

void PackageManager::add_local_verily_version(
    const PackageVersion &_version,
    const std::filesystem::path &_path) const {
  const auto actual_path = std::filesystem::canonical(
      std::filesystem::current_path() / _path);
  const auto cur_path = std::filesystem::current_path();

  if (!std::filesystem::exists(packages_path)) {
    std::filesystem::create_directory(packages_path);
  }
  std::filesystem::current_path(packages_path);
  if (!std::filesystem::exists(version)) {
    std::filesystem::create_directory(version);
  }

  if (!std::filesystem::exists(_path / "verily.out")) {
    throw std::runtime_error(
        "Cannot add local verily version where verily "
        "executable is missing");
  } else if (!std::filesystem::exists(_path / "hark.out")) {
    throw std::runtime_error(
        "Cannot add local verily version where hark "
        "executable is missing");
  }

  std::filesystem::copy(_path / "verily.out", packages_path /
                                                  _version /
                                                  "verily.out");
  std::filesystem::copy(_path / "hark.out",
                        packages_path / _version / "hark.out");

  std::filesystem::current_path(cur_path);
}
