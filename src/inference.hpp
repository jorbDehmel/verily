/**
 * @brief
 */

#pragma once

#include "ast.hpp"
#include "congruence.hpp"
#include <cstdint>
#include <map>
#include <optional>

/// A maker of inferences. It takes rules and axioms and deduces
/// theorems
class InferenceMaker {
public:
  /// If true, prints some extra info
  bool debug = false;

  /// If true, suppresses warning and error messages
  bool quiet = false;

  /// If true, forward_prove and backward_prove can call each
  /// other.
  bool enable_alternation = false;

  /// If true, attempts to use metalogical techniques.
  bool meta_proving = true;

  /// Don't mess with. Internally used during alternation.
  bool cur_alternation_is_forward = false;

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
    InferenceRule(const ASTSet &_fv,
                  const std::vector<ASTNode> &_req,
                  const ASTNode &_cons);

    /// The free variables over both the requirements and the
    /// consequence
    ASTSet free_variables;

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

    /// Substitute the given node for the first requirement and
    /// return the result
    std::optional<InferenceRule>
    remove_first_req(const ASTNode &_sub) const noexcept;

    /// Throws on failure
    ASTNode apply(const std::vector<Theorem> &_premises,
                  const uintmax_t &_fresh_num) const;
  };

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

  /// Prove using settings. This is the one you should call.
  std::optional<Theorem> prove(const ASTNode &_theorem,
                               const int &_passes);

  /// Adds an axiom and returns its index
  size_t add_axiom(const ASTNode &_what) noexcept;

  /// Gets a rule by internal index
  const InferenceRule get_rule(const uint &_index) const;

  /// Gets a rule by name
  const InferenceRule get_rule(const std::string &_name,
                               size_t &_index) const;

  /// Find the given theorem and name it
  void name_theorem(const ASTNode &_what,
                    const std::string &_name);

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

  /// Iterates through all possible theorem choices and
  /// instantiates wherever possible. Note that this only looks
  /// at theorems from the first n of them.
  void inst_all(const size_t &_rule_index,
                const size_t &_first_n_thms,
                const std::vector<size_t> &_cur_indices = {});

  /// Statements which are known to be true
  std::vector<Theorem> known;

  /// Statements currently being proven
  std::vector<ASTNode> pending;

  /// Inference rules
  std::vector<InferenceRule> rules;

  struct BackupFrame {
    std::vector<Theorem> theorems;
    std::vector<ASTNode> pending;
    std::vector<InferenceRule> rules;
  };
  std::list<BackupFrame> backup_frames;

  /// Push a frame such that any theorems will not be saved
  /// after popping
  inline void push() noexcept {
    backup_frames.push_back(BackupFrame(known, pending, rules));
  }

  /// Pop the most recent frame (if there is one)
  inline void pop() noexcept {
    if (!backup_frames.empty()) {
      known = backup_frames.back().theorems;
      pending = backup_frames.back().pending;
      rules = backup_frames.back().rules;
      backup_frames.pop_back();
    }
  }

  /// Pop the given item, warning if it is not the current back
  inline void pop_pending(const ASTNode &_expected_back) {
    std::erase_if(pending, [&](const ASTNode &_item) -> bool {
      return _item == _expected_back;
    });
  }

  /// Returns true iff the given theorem is currently pending
  inline bool is_pending(const ASTNode &_thm) {
    for (auto rit = pending.rbegin(); rit != pending.rend();
         ++rit) {
      if (*rit == _thm) {
        return true;
      }
    }
    return false;
  }

  /// The max number of theorems to allow before emergency stop
  uintmax_t theorem_limit = 10'000;

  /// The highest tree height that is allowed to be checked
  uintmax_t max_tree_height = 128;

  /// Maps theorem IDs to custom proofs (EG metalogical ones)
  std::map<int, ASTNode> special_proofs;

  /// Used to keep track of congruence classes if metalogical
  /// proofs are enabled.
  CongruenceKeeper congruences;
};

/// Print a rule
std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::InferenceRule &);

/// Print a theorem
std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::Theorem &);
