/**
 * @brief Stack management, mostly for goals / tactics
 */

#pragma once

#include "ast.hpp"
#include <cstdint>
#include <optional>

/// A maker of inferences. It takes rules and axioms and deduces
/// theorems
class InferenceMaker {
public:
  /// A statement, along with proof that it is a theorem
  struct Theorem {
    /// If given, the name of the theorem.
    std::optional<std::string> name;

    /// The internal index of this theorem
    size_t index;

    /// The syntactic representation of this theorem
    ASTNode thm;

    /// Either the index of the rule causing this theorem or
    /// a negative number (indicating an axiom).
    intmax_t rule_index;

    /// The indices of the theorems which satisfied the rule to
    /// create this. This might be empty.
    std::vector<size_t> premises;
  };

  /// If all the requirements are met, the consequences are
  /// implied
  struct InferenceRule {
    /// If given, the name of the rule.
    std::optional<std::string> name;

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

  ASTNode proof_to_ast(const size_t &_thm_index) const;

  /// Given a theorem which has already been proven, return its
  /// proof as a new AST.
  ASTNode theorem_to_proof(const ASTNode &_theorem) const;

  /// Adds a new rule
  void add_rule(const InferenceRule &_rule);

  /// Returns nonnegative iff _what has ALREADY been derived.
  /// Return value is -1 for underived, else index of proven
  /// theorem.
  int has(const ASTNode &_what) const noexcept;

  /// Adds an axiom and returns its index
  size_t add_axiom(const ASTNode &_what) noexcept;

  /// Gets a rule by internal index
  const InferenceRule get_rule(const uint &_index) const;

  /// Gets a rule by name
  const InferenceRule get_rule(const std::string &_name,
                               size_t &_index) const;

  /// Gets a theorem by internal index
  const Theorem get_theorem(const uint &_index) const;

  /// Gets a theorem by name
  const Theorem get_theorem(const std::string &_name) const;

  /// Adds a theorem
  const Theorem
  add_theorem(const ASTNode &_thm,
              const std::optional<std::string> &_name,
              const size_t &_rule_index,
              const std::vector<size_t> &_premises,
              bool &_actually_added);

  /// All the information needed to push or pop a snapshot
  struct BackupFrame {
    /// The known theorems at the time
    std::vector<Theorem> theorems;

    /// The known pending theorems at the time
    std::vector<ASTNode> pending;

    /// The known rules at the time
    std::vector<InferenceRule> rules;
  };

  /// Push a frame such that any theorems will not be saved
  /// after popping
  inline void push() noexcept {
    backup_frames.push_back(BackupFrame(facts, goals, rules));
  }

  /// Pop the most recent frame (if there is one)
  inline void pop() noexcept {
    if (!backup_frames.empty()) {
      facts = backup_frames.back().theorems;
      goals = backup_frames.back().pending;
      rules = backup_frames.back().rules;
      backup_frames.pop_back();
    }
  }

  /// Pop the given item, warning if it is not the current back
  inline void pop_pending(const ASTNode &_expected_back) {
    std::erase_if(goals, [&](const ASTNode &_item) -> bool {
      return _item == _expected_back;
    });
  }

  /// Returns true iff the given theorem is currently pending
  inline bool is_pending(const ASTNode &_thm) {
    for (auto rit = goals.rbegin(); rit != goals.rend();
         ++rit) {
      if (*rit == _thm) {
        return true;
      }
    }
    return false;
  }

  /// Checks a proof, obviously. It recurses and logs its
  /// successes as needed
  void check_proof(const ASTNode &_goal, const ASTNode &_proof,
                   const std::string &_name);

  /// Statements which are known to be true
  std::vector<Theorem> facts;

  /// Statements currently being proven
  std::vector<ASTNode> goals;

  /// Inference rules
  std::vector<InferenceRule> rules;

  /// Backup frames, used by push and pop
  std::list<BackupFrame> backup_frames;

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
