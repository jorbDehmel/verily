#pragma once
#include "inference.hpp"
#include "parse.hpp"
#include <cstdint>
#include <iostream>

/// A filepath used when none is provided
const static std::filesystem::path null_fp =
    "NO_FP_GIVEN.verily";

/// The core Verily management class
class Core {
public:
  /// Turns all underscores to spaces
  static std::string sanitize_name(const std::string &_s);

  /// Prints the rules, axioms, and selected theorems in latex
  /// 'inferrule' notation
  void latex(std::ostream &_strm) const;

  /// Prints the rules, axioms, and selected theorems in JSON
  /// encoding
  void json(std::ostream &_strm) const;

  /// Execute a statement
  void
  process_statement(const ASTNode &_stmt,
                    const std::filesystem::path &_cur_path);

  /// Do a file, executing each statement sequentially
  void do_file(const std::filesystem::path &_fp);

  /// List all rules and theorems
  void ls() const noexcept;

  InferenceMaker im;

  bool saw_error = false;
  bool debug = false;
  bool time = false;
  bool print_latex = false;
  bool print_json = false;
  uintmax_t pass_limit = 64;
  std::set<size_t> axioms;
  std::set<size_t> proven_theorems;
};
