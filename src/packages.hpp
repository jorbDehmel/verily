/**
 * @brief A package manager for verily, to be invoked by meta
 * statements.
 */

#pragma once

#include <compare>
#include <cstdlib>
#include <filesystem>
#include <list>
#include <map>
#include <optional>
#include <string>

/// For example: 1 > 0.1, 2 > 1.9.9.9. The empty version is 0,
/// which is less-than-or-equal-to everything.
struct PackageVersion {
  /// Most significant entries are at the front
  std::list<size_t> version;

  /// Determine order between two versions
  std::strong_ordering
  operator<=>(const PackageVersion &_other) const noexcept;

  /// Canonical repr
  operator std::string() const noexcept;

  /// As a filepath
  operator std::filesystem::path() const noexcept;

  /// Parse from a string
  static PackageVersion from_str(const std::string &_from);

  /// Get the version immediately after this one
  PackageVersion next() const noexcept;
};

/// Print a version
std::ostream &operator<<(std::ostream &_strm,
                         const PackageVersion &_version);

/// The current version of verily
const PackageVersion version({0, 1, 2});

/// Manages packages
class PackageManager {
public:
  /// Maps a path (EG std/foo.verily) to the actual
  /// location (EG /usr/lib/verily/0.1.0/std-0.1.0/foo.verily).
  /// Gets the highest version matching the given criteria.
  std::filesystem::path
  find(const std::filesystem::path &_path,
       const std::filesystem::path &_current_path);

  /// Install a package if it is not present. Returns true if
  /// it did anything.
  bool add_package(const std::string &_name,
                   const PackageVersion &_version,
                   const std::string &_git_repo_url);

  /// Install a package if it is not present. Returns true if
  /// it did anything. This copies from a local path.
  bool add_local_package(const std::string &_name,
                         const PackageVersion &_version,
                         const std::filesystem::path &_path);

  /// Remove a package if it is present. Returns true if it did
  /// anything.
  bool remove_package(const std::string &_name,
                      const PackageVersion &_version);

  /// True iff it exists at all
  bool has(const std::string &_name) const noexcept;

  /// True iff it exists with given version
  bool has(const std::string &_name,
           const PackageVersion &_version) const noexcept;

  /// Get all versions which are available
  std::list<PackageVersion>
  all_versions(const std::string &_name) noexcept;

  /// Restrict that, if we use _name, it must be of version
  /// strictly less-than _version
  void restrict_version_lt(const std::string &_name,
                           const PackageVersion &_version);

  /// Restrict that, if we use _name, it must be of version
  /// greater-than-or-equal-to _version
  void restrict_version_ge(const std::string &_name,
                           const PackageVersion &_version);

  /// Install a version of verily from a url
  void add_verily_version(const std::string &_url) const;

  /// Install a version of verily from a local path
  void add_local_verily_version(
      const std::filesystem::path &_path) const;

  /// Remove a version of verily, as well as all package data
  /// associated with that version
  void remove_verily_version(
      const PackageVersion &_version) const noexcept;

  /// Set the current version of verily / hark
  void set_verily_version(const PackageVersion &_version) const;

  /// Get a list of all installed verily versions
  std::list<PackageVersion>
  all_verily_versions() const noexcept;

  /// The root dir where we keep all the package info
  const std::filesystem::path packages_path = "/usr/lib/verily";

  /// Where we put executables
  const std::filesystem::path executables_path = "/usr/bin";

protected:
  /// Maps a package name to a 2-tuple of (lowest_allowed,
  /// lowest_too_high)
  std::map<std::string,
           std::pair<std::optional<PackageVersion>,
                     std::optional<PackageVersion>>>
      constraints;

  /// Cache for all_versions
  std::map<std::string, std::list<PackageVersion>>
      all_versions_cache;

  /// Cache for find
  std::map<
      std::pair<std::filesystem::path, std::filesystem::path>,
      std::filesystem::path>
      find_cache;
};
