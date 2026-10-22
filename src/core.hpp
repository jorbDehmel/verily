#pragma once

#include "inference.hpp"
#include "parse.hpp"
#include <filesystem>
#include <functional>
#include <iostream>

/// A filepath used when none is provided
const static std::filesystem::path null_fp =
    "NO_FP_GIVEN.verily";

/// The core Verily management class
class Core {
public:
  Core(const bool &_debug = false) : debug(_debug) {
    parser.debug = _debug;
  }

  /// Invoked when parse_and_process is called
  Parser parser;

  /// We will parse the given string next.
  inline void
  prepend_parse(const std::string &_str,
                const std::filesystem::path &_cur_path) {
    const auto tokens = lex_text(_str, _cur_path).data;
    parser.ts.prepend(tokens);
  }

  /// We will parse the given string after exhausting all
  /// existing data.
  inline void
  append_parse(const std::string &_str,
               const std::filesystem::path &_cur_path) {
    const auto tokens = lex_text(_str, _cur_path).data;
    parser.ts.append(tokens);
  }

  /// Parse and process a statement
  inline ASTNode
  parse_and_process(const std::filesystem::path &_cur_path) {
    const auto stmt = parser.parse_statement();
    if (debug) {
      std::cout << "Parsed " << stmt << ", now processing.\n";
    }
    process_statement(stmt, _cur_path);
    return stmt;
  }

  /// Turns all underscores to spaces
  static std::string sanitize_name(const std::string &_s);

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

  /// The set of files to blacklist from visiting again
  std::set<std::filesystem::path> visited_files;

  /// Called by 'setting' / 'option' statements. This is for
  /// the CLI to handle.
  std::function<void(Core &, const std::string &)>
      handle_setting =
          [](Core &, const std::string &_s) -> void {
    std::cout << "Unhandled setting: " << _s << "\n";
  };
};
