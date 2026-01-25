// Lexes and parses verily.

#pragma once

#include <compare>
#include <cstdint>
#include <filesystem>
#include <list>
#include <set>
#include <string>
#include <sys/types.h>
#include <vector>

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
  bool operator==(const Token &_other) const noexcept;
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
  bool done() const noexcept;

  /// Get the current token
  Token cur() const noexcept;

  /// Advance to the next token
  void next();

  /// Get the current token, then advance to the next one
  Token cur_next();

  /// Assert that the current token is in 'what' and advance
  void expect(std::set<std::string> what);
};

TokenStream lex_text(const std::string &text,
                     const std::filesystem::path &fp);

TokenStream lex_file(const std::filesystem::path &fp);

/// A single node in an Abstract Syntax Tree (AST)
struct ASTNode {
  /// The text of this node
  Token text;

  /// The children of this node
  std::vector<ASTNode> children;

  /// Construct with some text and children
  ASTNode(const Token &_text = {},
          const std::vector<ASTNode> &_children = {});

  /// True iff the text is equivalent and all the children are
  bool operator==(const ASTNode &_other) const noexcept;

  /// True iff the root text is _other
  bool operator==(const std::string &_other) const noexcept;

  /// True iff this node matches the other or any of its
  /// children do
  bool contains(const ASTNode &_what) const noexcept;

  /// True iff this node's text matches or recurse on children
  bool contains(const std::string &_what) const noexcept;

  /// Used for mapping
  std::strong_ordering
  operator<=>(const ASTNode &_other) const noexcept;

  /// Returns a COPY of this node, but with any instances
  /// equivalent to _to_replace replaced with _replace_with.
  ASTNode replace(const ASTNode &_to_replace,
                  const ASTNode &_replace_with) const noexcept;

  /// Equivalent to repeatedly single-replacing the AST
  ASTNode replace(const std::list<std::pair<ASTNode, ASTNode>>
                      &_replacements) const noexcept;

  /// Recursively apply all beta reductions ALREADY present in
  /// the tree. This could enter an infinite loop (but will
  /// likely stack-overflow instead)!
  ASTNode beta_star() const noexcept;
};

std::ostream &operator<<(std::ostream &_strm,
                         const ASTNode &_node);

void fancy_print(std::ostream &_strm, const ASTNode &_node,
                 const uint &_depth = 0);

/// An object which takes a token stream and produces an AST
class Parser {
public:
  /// If true, prints some extra info
  bool debug = false;

  /// The token stream we are looking at
  TokenStream ts;

  /// Construct from a given token stream
  Parser(const TokenStream &_ts);

  /// Parses a global scope
  ASTNode parse();

  /// Parses a single statement
  ASTNode parse_statement();

  /// Parses a set
  /// This could be like 'Nat'
  /// Or it could be like 'Nat to Bool to Int to Nat'
  ASTNode parse_set();

  /// Parses a series of arguments
  ASTNode parse_args();

  /// Parses a requirement / ensures block
  ASTNode parse_req_ens();

  /// Parses a (functional) function definition
  ASTNode parse_function();

  /// Parses an annotation. This can be used in either the
  /// global context or within an imperative block.
  ASTNode parse_theorem();

  /// Parses an imperative method definition
  ASTNode parse_method();

  /// Parses an expression in colinear time WRT number of
  /// operations, number of tokens in expression. This is a
  /// little baby push-reduce + linear recursive descent
  /// parser all of its own.
  ASTNode parse_expr();

  /// Parse an expression from a list
  ASTNode
  parse_expr_from_list(const std::list<ASTNode> &input_items);
};
