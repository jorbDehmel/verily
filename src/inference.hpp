/**
 * @brief Stack management, mostly for goals / tactics
 */

#pragma once

#include "ast.hpp"
#include <iostream>
#include <map>
#include <optional>

/// A maker of inferences. It takes rules and axioms and deduces
/// theorems
class InferenceMaker {
public:
  /// A statement, along with proof that it is a theorem
  struct Theorem {
    /// If given, the name of the theorem.
    std::string name;

    /// The syntactic representation of this theorem
    ASTNode thm;

    /// The rule that created this
    std::string rule_name;

    /// The indices of the theorems which satisfied the rule to
    /// create this. This might be empty.
    std::vector<std::string> premises;
  };

  /// If all the requirements are met, the consequences are
  /// implied
  struct InferenceRule {
    /// The name of the rule.
    std::string name;

    /// Construct an empty inference rule
    InferenceRule() {
    }

    /// Construct an inference rule
    InferenceRule(const std::set<ASTNode> &_fv,
                  const std::vector<ASTNode> &_req,
                  const ASTNode &_cons);

    /// The free variables over both the requirements and the
    /// consequence
    std::set<ASTNode> free_variables;

    /// The things which must be known theorems
    std::vector<ASTNode> requirements;

    /// Given the requirements over some substitutions, derive
    /// this theorem
    ASTNode consequence;

    /// This is a measure of where the free variables occur. If
    /// a rule is neither, it is an error.
    enum Type {
      FORWARD_ONLY,  /// Given premises, deduce conclusion
      BACKWARD_ONLY, /// Given conclusion, attempt premises
      BIDIRECTIONAL, /// Either way works
    };

    /// The type of this rule
    Type type = BACKWARD_ONLY;

    /// Throws on failure
    ASTNode apply(const std::vector<Theorem> &_premises) const;
  };

  void backward_apply(const InferenceRule &_rule,
                      const ASTNode &_goal);

  ASTNode proof_to_ast(const std::string &_theorem_name) const;

  /// Adds a new rule
  void add_rule(const InferenceRule &_rule);

  /// True iff we have already proven _what
  bool has(const ASTNode &_what) const noexcept;

  std::string get_name(const ASTNode &_what) const;

  /// Adds an axiom and returns its index
  void add_assumption(const std::string &_name,
                      const ASTNode &_what) noexcept;

  /// Gets a rule by name
  const InferenceRule get_rule(const std::string &_name) const;

  /// Gets a theorem by name
  const Theorem get_theorem(const std::string &_name) const;

  /// Adds a theorem
  const Theorem add_theorem(const Theorem &_thm);

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
          std::cerr << "Unresolved goal: " << goal << '\n';
          throw std::runtime_error("Unresolved goal!");
        }
      }

      frames.pop_back();
    }
  }

  /// Add a goal
  inline void open_goal(const ASTNode &_goal) {
    frames.back().goals.push_front(_goal);
  }

  /// Remove a goal (if it was present)
  inline void close_goal(const ASTNode &_goal) {
    std::erase_if(frames.back().goals,
                  [&](const ASTNode &_item) -> bool {
                    return _item == _goal;
                  });
  }

  /// Checks a proof, obviously. It recurses and logs its
  /// successes as needed
  void check_proof(const ASTNode &_goal, const ASTNode &_proof,
                   const std::string &_name);

  /// Where all the data is stored
  std::list<Frame> frames = {Frame{}};

  /// If true, prints some extra info
  bool debug = false;

  /// If true, suppresses warning and error messages
  bool quiet = false;
};

/// Print a rule
std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::InferenceRule &);

/// Print a theorem
std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::Theorem &);
