/// Lexes and parses verily.

#pragma once

#include "ast.hpp"

void fancy_print(std::ostream &_strm, const ASTNode &_node,
                 const uint &_depth = 0);

/// Given some text from some file, lex it
TokenStream lex_text(const std::string &text,
                     const std::filesystem::path &fp);

/// Load and lex a file's contents
TokenStream lex_file(const std::filesystem::path &fp);

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

  /// Parses an expression in colinear time WRT number of
  /// operations, number of tokens in expression. This is a
  /// little baby push-reduce + linear recursive descent
  /// parser all of its own.
  ASTNode parse_expr();

  /// Parse an expression from a list
  ASTNode
  parse_expr_from_list(const std::list<ASTNode> &input_items);

  /// Parses a Hoare method
  ASTNode parse_method();

  /// Parses a statement within a Hoare method
  ASTNode parse_method_statement();
};
