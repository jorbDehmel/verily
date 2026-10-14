/// Lexes and parses verily.

#pragma once

#include "ast.hpp"

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

  /// Implicit constructor
  Parser() {
  }

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

  /// Parse a proof, EG after 'by' in a theorem statement
  ASTNode parse_proof();

  /// Parse an expression from a list
  ASTNode
  parse_expr_from_list(const std::list<ASTNode> &input_items);

  /// Enough information to parse an operator
  struct OperationInfo {
    enum {
      PREFIX,
      INFIX, // INFIX is only allowed for binary
      POSTFIX,
    } mode = PREFIX;
    int arity = 1;
    std::string tok = "";
    std::string label = "";
  };

  /// The order of operations (if you can believe it)
  std::list<OperationInfo> order_of_operations = {};
};
