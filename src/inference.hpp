/**
 * @brief Stack management, mostly for goals / tactics
 */

#pragma once

#include "ast.hpp"
#include <iostream>
#include <map>

/// A maker of inferences. It takes rules and axioms and deduces
/// theorems
class InferenceMaker {
public:
  /// A statement, along with proof that it is a theorem
  struct Theorem {
    /// Empty construct
    Theorem() {
    }

    /// Construct
    Theorem(const ASTNode &_thm, const std::string &_name,
            const std::vector<ASTNode> &_premises)
        : thm(_thm), rule_name(_name), premises(_premises) {
    }

    /// The syntactic representation of this theorem
    ASTNode thm;

    /// The rule that created this
    std::string rule_name;

    /// The indices of the theorems which satisfied the rule to
    /// create this. This might be empty.
    std::vector<ASTNode> premises;
  };

  /// Apply, but backwards.
  void backward_apply(const std::string &_rule_name,
                      const InferenceRule &_rule,
                      const ASTNode &_goal);

  /// Return an AST representing the proof of the theorem with
  /// the given name
  ASTNode proof_to_ast(const std::string &_theorem_name) const;

  /// Adds a new rule
  void add_rule(const std::string &_name,
                const InferenceRule &_rule);

  /// True iff we have already proven _what
  bool has(const ASTNode &_what) const noexcept;

  /// Get the name of the proof, throwing if it doesn't exist
  std::string get_name(const ASTNode &_what) const;

  /// Adds an axiom and returns its index
  void add_assumption(const std::string &_name,
                      const ASTNode &_what) noexcept;

  /// Gets a rule by name
  const InferenceRule get_rule(const std::string &_name) const;

  /// Gets a theorem by name
  const Theorem get_theorem(const std::string &_name) const;

  /// Gets a theorem by theorem
  const Theorem get_theorem(const ASTNode &_thm) const;

  /// Adds a theorem
  const Theorem add_theorem(const std::string &_name,
                            const Theorem &_thm);

  /// Keeps all the info
  struct Frame {
    /// Statements which are known to be true
    std::map<std::string, Theorem> theorems;

    /// Statements currently being proven
    std::list<ASTNode> goals;

    /// Inference rules / axioms
    std::map<std::string, InferenceRule> rules;
  };

  /// Push a frame such that any theorems will not be saved
  /// after popping
  inline void push() noexcept {
    frames.push_back(Frame());
  }

  /// Pop the most recent frame (if there is one)
  inline void pop() {
    if (frames.size() > 1) {
      for (const auto &goal : frames.back().goals) {
        if (!has(goal)) {
          std::cerr << "Unresolved goal at end of frame: "
                    << goal << '\n';
          throw std::runtime_error(
              "Unresolved goal at end of frame");
        }
      }
      frames.pop_back();
    }
  }

  /// Add a goal
  inline void open_goal(const ASTNode &_goal) {
    if (!has(_goal)) {
      if (debug) {
        std::cout << "Adding goal " << _goal << '\n';
      }
      frames.back().goals.push_front(_goal);
    }
  }

  /// Remove a goal (if it was present)
  inline void close_goal(const ASTNode &_goal) {
    if (debug) {
      std::cout << "Dropping goal " << _goal << '\n';
    }
    std::erase_if(frames.back().goals,
                  [&](const ASTNode &_item) -> bool {
                    return _item == _goal;
                  });
  }

  /// Checks a proof, obviously. It recurses and logs its
  /// successes as needed
  void check_proof(const ASTNode &_goal, const ASTNode &_proof,
                   const std::string &_name);

  /// List everything
  void ls() const noexcept;

  /// Where all the data is stored
  std::list<Frame> frames = {Frame()};

  /// If true, prints some extra info
  bool debug = false;
};

/// Print a theorem
std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::Theorem &);
