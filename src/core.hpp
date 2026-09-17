#pragma once

#include "inference.hpp"
#include <cstdint>
#include <functional>
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
  void latex(std::ostream &_strm,
             const std::string &_mode = "tree") const;

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

  /// Manages known information
  InferenceMaker im;

  /// True iff an error occurred
  bool saw_error = false;

  /// If true, prints some extra info
  bool debug = false;

  /// The max number of passes before abandoning
  uintmax_t pass_limit = 64;

  /// The theorems which were explicitly requested
  std::set<size_t> proven_theorems;

  /// Called by 'setting' / 'option' statements. This is for
  /// the CLI to handle.
  std::function<void(Core &, const std::string &)>
      handle_setting =
          [](Core &, const std::string &_s) -> void {
    std::cout << "Unhandled setting: " << _s << "\n";
  };
};
