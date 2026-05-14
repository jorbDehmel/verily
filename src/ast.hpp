/**
 * @brief Abstract syntax trees, tokens, and token streams. This
 * is MIT-licensed copyware. Jordan Dehmel, 2026.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <list>
#include <set>
#include <string>
#include <sys/types.h>
#include <vector>

static_assert(__cplusplus >= 2020'00ULL,
              "C++ 2020 or later required");

/// A token with metadata about its type and location
struct Token {
  /// The text at this file location
  std::string text = "";

  /// The file this token came from
  std::filesystem::path file = "N/A";

  /// The line within the file
  uintmax_t line = 0;

  /// The column within the line within the file
  uintmax_t col = 0;

  /// Construct a token
  Token(const std::string &_t = "",
        const std::filesystem::path &_f = "",
        const uintmax_t &_l = 0, const uintmax_t &_c = 0)
      : text(_t), file(_f), line(_l), col(_c) {
  }

  /// Lower-level constructor, because for some reason this
  /// isn't implicitly derived
  Token(const char *_c_str) : Token(std::string(_c_str)) {
  }

  /// True iff the texts are the same (does NOT need to be at
  /// the same location)
  inline bool operator==(const Token &_other) const noexcept {
    return text == _other.text;
  }
};

/// A sequence of tokens used in parsing
class TokenStream {
public:
  /// The tokens
  std::vector<Token> data;

  /// The current index into data
  uintmax_t pos;

  /// Initialize to the beginning of the token list
  TokenStream(const std::vector<Token> &_tokens)
      : data(_tokens), pos(0) {
  }

  /// True iff we have advanced passed the end of the stream
  inline bool done() const noexcept {
    return pos >= data.size();
  }

  /// Get the current token
  inline Token cur() const noexcept {
    if (pos >= data.size()) {
      return Token("EOF");
    }
    return data.at(pos);
  }

  /// Advance to the next token
  inline void next() {
    ++pos;
  }

  /// Get the current token, then advance to the next one
  inline Token cur_next() {
    const auto out = cur();
    next();
    return out;
  }

  /// Assert that the current token is in 'what' and advance
  void expect(std::set<std::string> what);
};

class ASTSet;

/// A single node in an Abstract Syntax Tree (AST)
struct ASTNode {
  /// The text of this node
  Token text;

  /// The children of this node
  std::vector<ASTNode> children;

  /// Construct with some text and children
  ASTNode(const Token &_text = "",
          const std::vector<ASTNode> &_children = {})
      : text(_text), children(_children) {
    if (text.file == "N/A") {
      for (const auto &child : children) {
        if (child.text.file != "N/A") {
          text.file = child.text.file;
          text.line = child.text.line;
          text.col = child.text.col;
          break;
        }
      }
    }
  }

  ASTNode copy() const noexcept {
    ASTNode out;
    out.text = text;
    for (const auto &child : children) {
      out.children.push_back(child.copy());
    }
    return out;
  }

  /// True iff the text is equivalent and all the children are
  bool operator==(const ASTNode &_other) const noexcept;

  /// True iff the root text is _other
  inline bool
  operator==(const std::string &_other) const noexcept {
    return text.text == _other;
  }

  /// True iff this node matches the other or any of its
  /// children do
  bool contains(const ASTNode &_what) const noexcept;

  /// True iff this node's text matches or recurse on children
  bool contains(const std::string &_what) const noexcept;

  /// Returns a COPY of this node, but with any instances
  /// equivalent to _to_replace replaced with _replace_with.
  ASTNode replace(const ASTNode &_to_replace,
                  const ASTNode &_replace_with) const noexcept;

  /// Many replacements (in precedent order). Returns a copy.
  ASTNode replace(const std::list<std::pair<ASTNode, ASTNode>>
                      &_replacements) const noexcept;

  /// Return the max height of the tree
  uintmax_t get_height() const noexcept {
    uintmax_t out = 0;
    for (const auto &child : children) {
      out = std::max(out, child.get_height() + 1);
    }
    return out;
  }

  /// Returns true iff _to_examine is of the form _form with
  /// free
  /// variables _free_variables (whose substitutions are logged
  /// in _substitutions).
  bool is_of_form(const ASTNode &_form, ASTSet &_free_variables,
                  std::list<std::pair<ASTNode, ASTNode>>
                      &_substitutions) const;
};

/// Prints an AST node as an S-expression
std::ostream &operator<<(std::ostream &_strm,
                         const ASTNode &_node);

/// A (naive, linear search) set of AST nodes
class ASTSet {
public:
  /// O(n)
  inline void insert(const ASTNode &_what) noexcept {
    if (!contains(_what)) {
      data.push_back(_what);
    }
  }

  /// O(n)
  inline bool contains(const ASTNode &_what) const noexcept {
    for (const auto &element : data) {
      if (element == _what) {
        return true;
      }
    }
    return false;
  }

  /// O(n)
  inline void erase(const ASTNode &_what) noexcept {
    std::erase_if(data, [&](auto cur) -> bool {
      return cur == _what;
    });
  }

  /// O(1)
  inline std::list<ASTNode>::const_iterator
  begin() const noexcept {
    return data.cbegin();
  }

  /// O(1)
  inline std::list<ASTNode>::const_iterator
  end() const noexcept {
    return data.cend();
  }

  /// O(1)
  inline bool empty() const noexcept {
    return data.empty();
  }

  /// O(n)
  inline void clear() noexcept {
    data.clear();
  }

protected:
  std::list<ASTNode> data;
};
