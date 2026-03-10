/**
 * @brief Theory checker for Equality and Uninterpreted
 * Functions (EUF) using equivalence classes. This works
 * directly on parsed call trees (EG "(CALL ... (ARGS ...))").
 */

#pragma once

#include "parse.hpp"
#include <map>
#include <optional>

/// Associates ints in an equivalence relation (efficiently).
/// This is weighted union find with path compression.
class UnionFind {
protected:
  /// Maps a node to its parent
  std::map<int, int> parents;

  /// Maps a root node to the size of its tree. Used for
  /// weighted path compression
  std::map<int, int> root_to_size;

  /// Gets the equivalence class ID associated with the given
  /// node
  int root(const int &_n);

public:
  /// Reset all relationships
  void reset() noexcept;

  /// Signal that these two nodes are related. Relatedness is
  /// an equivalence relation. A node that hasn't been seen
  /// before is only related to itself.
  void relate(const int &_a, const int &_b);

  /// Queries whether two nodes are in the same equivalence
  /// class.
  bool are_related(const int &_a, const int &_b);
};

/// A specialized version of union find. It partitions the set
/// of all ASTs into congruence classes.
class CongruenceKeeper {
protected:
  /// Maps a node to its ID
  std::list<std::pair<ASTNode, int>> key_to_id;

  /// Used to relate IDs of nodes
  UnionFind uf;

  size_t get_id(const ASTNode &) noexcept;

public:
  /// Reset all relations
  void reset();

  /// Add the fact that _a is related to _b
  void relate(const ASTNode &_a, const ASTNode &_b);

  /// Returns true if the two given nodes are known to be
  /// related (by congruence properties)
  bool are_related(const ASTNode &_a, const ASTNode &_b);
};
