/// CDCL

#pragma once

#include <functional>
#include <list>
#include <math.h>
#include <optional>
#include <ostream>
#include <set>

/**
 * @brief A CNF formula
 */
class CNF {
public:
  /// A single CNF clause
  using CNFClause = std::set<int>;

  /// Where the CNF formula (x1 or not x2) and (x2 or not x3)
  /// would be clauses=[[1, -2], [2, -3]]
  CNF(const std::vector<CNFClause> &_clauses = {});

  /// Evaluate the expression (sat, unsat, or none) given some
  /// assignments
  std::optional<bool>
  evaluate(const std::set<int> &_assignments) const noexcept;

  /// O(n), where n is the number of clauses in this + other
  CNF operator&&(const CNF &_other) const noexcept;

  /// True iff n_clauses() == 0
  bool empty() const noexcept;

  /// Gets the number of clauses
  uint n_clauses() const noexcept;

  /// Gets the variables
  std::set<int> vars() const noexcept;

  /// Gets the number of variables
  uint n_vars() const noexcept;

  /// The CNF clauses we operate on
  std::vector<CNFClause> clauses;
};

/// Returns 0 if impossible
int resolvent_pivot(const CNF::CNFClause &_l,
                    const CNF::CNFClause &_r) noexcept;

/// Return the resolvent, throwing if impossible
CNF::CNFClause resolve(const CNF::CNFClause &_l,
                       const CNF::CNFClause &_r,
                       const int &_pivot) noexcept;

std::ostream &operator<<(std::ostream &_into, const CNF &_what);

/// Assert that no assignment exists satisfying phi by brute
/// force. If the number of variables is above max_vars (50 by
/// default), this will balk without checking (since the
/// expected running time is likely longer than a lifetime).
void ensure_unsat(CNF phi, uint max_vars = 50);

/// Detects the first unit literal and eliminates it from any
/// clauses. More unit literals may remain after this. This
/// REMOVES the unit clause.
/// :param s: The CNF formula to operate on
/// :returns: The modified formula
std::vector<CNF::CNFClause>
unit_propagate(std::vector<CNF::CNFClause> s);

/**
 * @brief A CDCL solver with hooks for SMT. The only thing
 * missing for SMT is a formula abstraction cache.
 */
class CDCL {
public:
  /// An entry in the assignment stack
  struct AssignmentStackEntry {
    /// The asserted literal
    int lit;

    /// The reason this was asserted
    uint reason;

    /// The assumption level
    uint lvl;
  };

  /// An entry that *will* be assigned, but hasn't been yet
  struct AssignmentQueueEntry {
    /// The literal to assert
    int lit;

    /// Why to assert this literal
    uint reason;
  };

  /// The clause database
  CNF clauses;

  /// The assignments so far
  std::list<AssignmentStackEntry> assignment_stack;

  /// The assignments to (in the future) assign
  std::list<AssignmentQueueEntry> assignment_queue;

  /// Either nothing, or the conflict-learned clause from the
  /// SMT solver
  std::optional<std::set<int>> theory_clc = {};

  /// A function interfacing with SMT solvers. Return a value
  /// upon theory conflict, nothing upon theory non-conflict.
  /// The value returned will be stored as the theory
  /// conflict-learned-clause and will be naively used as a
  /// blocking clause.
  std::function<std::optional<std::set<int>>(
      const std::set<int> &)>
      theory_check = [](auto) -> std::optional<std::set<int>> {
    return {};
  };

  /// Initialize with some formula
  CDCL(CNF phi);

  /// Returns true unless no next assignment can be found
  bool choose_next_assignment();

  /// Return the backtracking level
  int analyze_conflict();

  /// Roll back some decisions
  void backtrack(uint to_lvl);

  /// Gets rid of as much complexity as it can without
  /// performing CDCL
  void preprocess();

  /// Returns true if conflict
  bool deduce();

  /// Returns assignment for SAT, {} for UNSAT
  std::optional<std::set<int>> go();
};
