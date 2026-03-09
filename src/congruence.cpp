#include "congruence.hpp"

void ASTNodeToIDMap::serialize(
    const ASTNode &_what,
    std::list<std::string> &_cur) const noexcept {
  _cur.push_back("OPEN");
  _cur.push_back(_what.text.text);
  for (const auto &child : _what.children) {
    serialize(child, _cur);
  }
  _cur.push_back("CLOSE");
}

int ASTNodeToIDMap::at(const ASTNode &_ast) {
  std::list<std::string> serialized;
  serialize(_ast, serialized);
  TSTNode &cur = root;
  for (const auto &entry : serialized) {
    cur = cur.next[entry];
  }
  if (!cur.data.has_value()) {
    cur.data = ++cur_size;
  }
  return cur.data.value();
}

int UnionFind::root(const int &_n) {
  // Special case: First time a node has been mentioned
  if (!parents.contains(_n)) {
    parents[_n] = _n;
    root_to_size[_n] = 1;
    return _n;
  }

  // Pass 1: Find root
  int root = _n;
  while (root != parents[root]) {
    root = parents[root];
  }

  // Pass 2: Path compression
  int cur = _n;
  while (cur != parents[cur]) {
    const int next = parents[cur];
    parents[cur] = root;
    cur = next;
  }

  return root;
}

void UnionFind::reset() noexcept {
  for (const auto &p : parents) {
    parents[p.first] = p.first;
    root_to_size[p.first] = 1;
  }
}

/// Signal that these two nodes are related. Relatedness is
/// an equivalence relation. A node that hasn't been seen
/// before is only related to itself.
void UnionFind::relate(const int &_a, const int &_b) {
  if (_a == _b) {
    return;
  }

  const int a_root = root(_a);
  const int b_root = root(_b);

  // Weighted union
  if (root_to_size[a_root] > root_to_size[b_root]) {
    // b is smaller than a: Make b point to a
    parents[b_root] = a_root;
    root_to_size[a_root] += root_to_size[b_root];
    root_to_size.erase(b_root);
  } else {
    // a is smaller than b: Make a point to b
    parents[a_root] = b_root;
    root_to_size[b_root] += root_to_size[a_root];
    root_to_size.erase(a_root);
  }
}

/// Queries whether two nodes are in the same equivalence
/// class.
bool UnionFind::are_related(const int &_a, const int &_b) {
  if (_a == _b) {
    return true;
  }
  return root(_a) == root(_b);
}

void CongruenceKeeper::reset() {
  uf.reset();
}

void CongruenceKeeper::relate(const ASTNode &_a,
                              const ASTNode &_b) {
  // std::cout << "Adding (" << _a << ") equiv (" << _b <<
  // ")\n";
  uf.relate(key_to_id.at(_a), key_to_id.at(_b));
}

bool CongruenceKeeper::are_related(const ASTNode &_a,
                                   const ASTNode &_b) {
  // std::cout << "Checking if (" << _a << ") equiv (" << _b
  //           << ")\n";

  const int a_id = key_to_id.at(_a);
  const int b_id = key_to_id.at(_b);

  // Raw + cheap case
  if (uf.are_related(a_id, b_id)) {
    return true;
  }

  // Else, check function relations
  if (_a.text.text == _b.text.text) {
    // If all the arguments are related
    if (_a.children.size() != _a.children.size()) {
      return false;
    }
    for (uint i = 0; i < _a.children.size(); ++i) {
      if (!are_related(_a.children.at(i), _b.children.at(i))) {
        return false;
      }
    }

    // Now we don't have to do this again
    uf.relate(a_id, b_id);
    return true;
  }

  return false;
}
