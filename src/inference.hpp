/**
 * @brief
 */

#pragma once

#include "../src/parse.hpp"
#include <cstdint>
#include <optional>
#include <set>

/// A maker of inferences. It takes rules and axioms and deduces
/// theorems
class InferenceMaker {
public:
  /// If true, prints some extra info
  bool debug = false;

  /// If true, forward_prove and backward_prove can call each
  /// other.
  bool enable_alternation = false;

  /// If all the requirements are met, the consequences are
  /// implied
  struct InferenceRule {
    /// If given, the name of the rule.
    std::optional<std::string> name;

    /// Construct an inference rule
    InferenceRule(const std::set<ASTNode> &_fv,
                  const std::list<ASTNode> &_req,
                  const ASTNode &_cons);

    /// The free variables over both the requirements and the
    /// consequence
    std::set<ASTNode> free_variables;

    /// The things which must be known theorems
    std::list<ASTNode> requirements;

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

    /// Substitute the given node for the first requirement and
    /// return the result
    std::optional<InferenceRule>
    remove_first_req(const ASTNode &_sub) const noexcept;
  };

  /// A statement, along with proof that it is a theorem
  struct Theorem {
    /// The internal index of this theorem
    const size_t index;

    /// The syntactic representation of this theorem
    const ASTNode thm;

    /// Either the index of the rule causing this theorem or
    /// a negative number (indicating an axiom).
    const intmax_t rule_index;

    /// The indices of the theorems which satisfied the rule to
    /// create this. This might be empty.
    const std::list<size_t> premises;
  };

  /// Returns true iff _to_examine is of the form _form
  /// with
  /// free variables _free_variables (whose substitutions
  /// are logged in _substitutions).
  static bool is_of_form(
      const ASTNode &_to_examine, const ASTNode &_form,
      std::set<ASTNode> &_free_variables,
      std::list<std::pair<ASTNode, ASTNode>> &_substitutions);

  /// Adds a new rule
  void add_rule(const InferenceRule &_rule);

  /// Returns nonnegative iff _what has ALREADY been derived.
  /// Return value is -1 for underived, else index of proven
  /// theorem.
  int has(const ASTNode &_what) const noexcept;

  /// Attempt to prove the given statement backwards (EG from
  /// implication to implicate-ee). This is NOT necessarily a
  /// decision procedure! Will halt after depth reaches _passes.
  std::optional<Theorem> backward_prove(const ASTNode &_what,
                                        const int &_passes);

  /// Attempt to prove the given statement forwards (EG from
  /// requirements to implication). This is NOT necessarily a
  /// decision procedure! Will try each forward-derivable rule
  /// in a round-robin manner until either _what is proven or
  /// _passes applications of each rule have occurred. There is
  /// no direction here!
  std::optional<Theorem> forward_prove(const ASTNode &_what,
                                       const int &_passes);

  /// Adds an axiom and returns its index
  size_t add_axiom(const ASTNode &_what) noexcept;

  /// Gets a rule
  const InferenceRule get_rule(const uint &_index) const;

  /// Gets a theorem
  const Theorem get_theorem(const uint &_index) const;

  /// Adds a theorem
  const Theorem add_theorem(const ASTNode &_thm,
                            const uint &_rule_index,
                            const std::list<size_t> &_premises,
                            bool &_actually_added);

  /// Iterates through all possible theorem choices and
  /// instantiates wherever possible. Note that this only looks
  /// at theorems from the first n of them.
  void inst_all(const uint &_rule_index,
                const uint &_first_n_thms,
                const std::vector<uint> &_cur_indices = {});

  /// Statements which are known to be true
  std::vector<Theorem> known;

  /// Inference rules
  std::vector<InferenceRule> rules;
};

std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::InferenceRule &);

std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::Theorem &);
