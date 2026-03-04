/// CDCL

#include "cdcl.hpp"
#include <map>

// Where the CNF formula (x1 or not x2) and (x2 or not x3)
// would be clauses=[[1, -2], [2, -3]]
CNF::CNF(const std::vector<CNFClause> &_clauses)
    : clauses(_clauses) {
  for (const auto &c : clauses) {
    if (c.contains(0)) {
      throw std::runtime_error("0 is not a valid CNF variable");
    }
  }
}

std::optional<bool> CNF::evaluate(
    const std::set<int> &_assignments) const noexcept {
  uint n_sat = 0;
  for (const auto &clause : clauses) {
    bool clause_sat = false;
    bool clause_unsat = true;

    for (const auto &lit : clause) {
      if (_assignments.contains(lit)) {
        clause_sat = true;
        break;
      } else if (clause_unsat && !_assignments.contains(lit)) {
        clause_unsat = false;
      }
    }

    if (clause_sat) {
      ++n_sat;
    } else if (clause_unsat) {
      return false;
    }
  }
  if (n_sat == clauses.size()) {
    return true;
  }
  return {};
}

// O(n), where n is the number of clauses in this + other
CNF CNF::operator&&(const CNF &_other) const noexcept {
  CNF out(clauses);
  for (const auto &c : _other.clauses) {
    out.clauses.push_back(c);
  }
  return out;
}

bool CNF::empty() const noexcept {
  return clauses.empty();
}

// O(n)
uint CNF::n_clauses() const noexcept {
  return clauses.size();
}

// O(n)
std::set<int> CNF::vars() const noexcept {
  std::set<int> all;
  for (const auto &c : clauses) {
    if (all.empty()) {
      all = c;
    } else {
      for (const auto &var : c) {
        all.insert(var);
      }
    }
  }

  return all;
}

// O(n)
uint CNF::n_vars() const noexcept {
  if (empty()) {
    return 0;
  }
  return vars().size();
}

/// Returns 0 if impossible
int resolvent_pivot(const CNF::CNFClause &_l,
                    const CNF::CNFClause &_r) noexcept {
  for (const auto &lit : _l) {
    if (_r.contains(-lit)) {
      return abs(lit);
    }
  }
  return 0;
}

/// Return the resolvent, throwing if impossible
CNF::CNFClause resolve(const CNF::CNFClause &_l,
                       const CNF::CNFClause &_r,
                       const int &_pivot) noexcept {
  CNF::CNFClause out;
  for (const auto &lit : _l) {
    if (abs(lit) != abs(_pivot)) {
      out.insert(lit);
    }
  }
  for (const auto &lit : _r) {
    if (abs(lit) != abs(_pivot)) {
      out.insert(lit);
    }
  }
  return out;
}

std::ostream &operator<<(std::ostream &_into,
                         const CNF &_what) {
  bool outer_first = true;
  for (const auto &clause : _what.clauses) {
    if (outer_first) {
      outer_first = false;
    } else {
      _into << " and ";
    }
    _into << "(";
    bool inner_first = true;
    for (const auto &lit : clause) {
      if (inner_first) {
        inner_first = false;
      } else {
        _into << " or ";
      }
      _into << lit;
    }
    _into << ")";
  }
  return _into;
}

void ensure_unsat(CNF phi, uint max_vars) {
  if (max_vars > 0 && phi.n_vars() > max_vars) {
    return;
  }

  std::set<int> variables_seen;
  std::list<int> variables;
  for (const auto &c : phi.clauses) {
    for (const auto &lit : c) {
      const int var = abs(lit);
      if (!variables_seen.contains(abs(var))) {
        variables_seen.insert(abs(var));
        variables.push_back(var);
      }
    }
  }

  std::set<int> assignments;
  bool running = true;
  while (running) {
    // Check this assignment
    if (phi.evaluate(assignments)) {
      throw std::runtime_error("Expected unsat, saw sat");
    }

    // Increment and update flag
    running = false;
    bool carry = true;
    for (const auto &var : variables) {
      const bool cur_val = assignments.contains(var);
      running |= !cur_val; // Run as long as at least 1 false

      // Increment
      if (!cur_val && carry) {
        assignments.insert(var);
      } else if (cur_val && carry) {
        assignments.erase(var);
      }

      carry &= cur_val; // Update carry
    }
  }
}

std::vector<CNF::CNFClause>
unit_propagate(std::vector<CNF::CNFClause> s) {
  for (const auto &c : s) {
    if (c.size() == 1) {
      const int pivot = *c.begin();
      std::vector<CNF::CNFClause> new_s;
      for (const auto &clause : s) {
        if (clause.contains(pivot)) {
          continue;
        } else if (clause.contains(-pivot)) {
          CNF::CNFClause to_add;
          for (const auto &v : clause) {
            if (v != -pivot) {
              to_add.insert(v);
            }
          }
          new_s.push_back(to_add);
        } else {
          new_s.push_back(clause);
        }
      }
      return new_s;
    }
  }
  return s;
}

CDCL::CDCL(CNF phi) : clauses(phi) {
  preprocess();
}

bool CDCL::choose_next_assignment() {
  int lvl = assignment_stack.empty()
                ? 1
                : assignment_stack.back().lvl;

  if (assignment_queue.size() >= 1) {
    const auto assignment = assignment_queue.front();
    assignment_queue.pop_front();
    assignment_stack.push_back(AssignmentStackEntry(
        assignment.lit, assignment.reason, lvl));
  } else {
    int chosen_var = 0;
    for (const auto &var : clauses.vars()) {
      chosen_var = var;
      for (const auto &entry : assignment_stack) {
        if (entry.lit == var or entry.lit == -var) {
          chosen_var = 0;
          break;
        }
      }
      if (chosen_var != 0) {
        break;
      }
    }

    if (chosen_var == 0) {
      return false;
    }

    assignment_stack.push_back(
        AssignmentStackEntry(chosen_var, 0, lvl + 1));
  }
  return true;
}

int CDCL::analyze_conflict() {
  for (const auto &c : clauses.clauses) {
    if (c.empty()) {
      return -1;
    }
  }

  std::set<int> assignments;
  for (const auto &entry : assignment_stack) {
    if (entry.lvl == 0) {
      if (assignments.contains(-entry.lit)) {
        return -1;
      }
      assignments.insert(entry.lit);
    }
  }

  uint uip_level = assignment_stack.back().lvl;

  if (uip_level == 0) {
    return -1;
  }

  std::map<int, uint> var_to_level;
  for (const auto &entry : assignment_stack) {
    var_to_level[abs(entry.lit)] = entry.lvl;
  }

  const auto valid_clc = [&](const CNF::CNFClause &s) -> bool {
    bool saw_clc = false;
    for (const auto &lit : s) {
      if (var_to_level.contains(abs(lit)) &&
          var_to_level.at(abs(lit)) == uip_level) {
        if (saw_clc) {
          return false;
        }
        saw_clc = true;
      }
    }
    return saw_clc;
  };

  const auto entry_to_clause =
      [&](const AssignmentStackEntry &entry) -> CNF::CNFClause {
    if (entry.reason == 0) {
      return CNF::CNFClause({entry.lit});
    } else {
      return clauses.clauses[entry.reason - 1];
    }
  };

  CNF::CNFClause out;

  if (theory_clc.has_value()) {
    out = theory_clc.value();
  } else {
    out = entry_to_clause(assignment_stack.back());
  }

  while (!valid_clc(out)) {
    auto cur_entry = assignment_stack.rbegin();
    int pivot = 0;

    while (pivot == 0) {
      const auto entry = *cur_entry;
      const auto against = entry_to_clause(entry);

      if (entry.lvl == uip_level && entry.reason == 0) {
        out = CNF::CNFClause({-entry.lit});
        break;
      }

      pivot = resolvent_pivot(out, against);
      if (pivot != 0) {
        out = resolve(out, against, pivot);
      }

      --cur_entry;
    }
  }

  uint backtracking_level = 0;
  for (const auto &v : out) {
    const auto l = var_to_level[abs(v)];
    if (l < uip_level && l > backtracking_level) {
      backtracking_level = l;
    }
  }

  clauses.clauses.push_back(out);
  return backtracking_level;
}

void CDCL::backtrack(uint to_lvl) {
  while (!assignment_stack.empty() &&
         assignment_stack.back().lvl > to_lvl) {
    assignment_stack.pop_back();
  }
}

void CDCL::preprocess() {
  bool check_again = true;
  while (check_again) {
    check_again = false;

    // Unit propagation
    for (const auto &clause : clauses.clauses) {
      if (clause.size() == 1) {
        const int lit = *clause.begin();
        clauses.clauses = unit_propagate(clauses.clauses);
        assignment_stack.push_front(
            AssignmentStackEntry(lit, 0, 0));
        check_again = true;
        break;
      }
    }
    if (check_again) {
      continue;
    }
  }
}

// Returns true if conflict
bool CDCL::deduce() {
  preprocess();

  std::set<int> assignments;
  for (const auto &a : assignment_stack) {
    assignments.insert(a.lit);
    if (assignments.contains(-a.lit)) {
      return true;
    }
  }

  int i = 0;
  for (const auto &c : clauses.clauses) {
    if (c.empty()) {
      return true;
    }

    bool is_sat = false;
    std::list<int> unassigned;

    uint j = 0;
    for (const auto &lit : c) {
      if (assignments.contains(lit)) {
        is_sat = true;
        break;
      }

      if (!assignments.contains(-lit) ||
          (unassigned.empty() && j + 1 == c.size())) {
        unassigned.push_back(lit);
      }
      ++j;
    }

    if (!is_sat && unassigned.size() == 1) {
      assignment_queue.push_back(
          AssignmentQueueEntry(*unassigned.begin(), i + 1));
      break;
    }

    ++i;
  }

  // Ensure we are up-to-date on assignments
  for (const auto &a : assignment_stack) {
    assignments.insert(a.lit);
  }

  // Check with theory solver
  theory_clc = theory_check(assignments);
  if (theory_clc.has_value()) {
    // Theory error!
    return true;
  }

  return false;
}

// Returns assignment for SAT, {} for UNSAT
std::optional<std::set<int>> CDCL::go() {
  while (choose_next_assignment()) {
    while (deduce()) {
      const int blevel = analyze_conflict();
      if (blevel < 0) {
        return {};
      } else {
        backtrack(blevel);
      }
    }
  }

  // Special case: unsat by preprocessing
  for (const auto &c : clauses.clauses) {
    if (c.empty()) {
      return {};
    }
  }

  std::set<int> out;
  for (const auto &a : assignment_stack) {
    out.insert(a.lit);
  }

  // Special case: unsat by preprocessing + theory
  theory_clc = theory_check(out);
  if (theory_clc.has_value()) {
    return {};
  }

  return out;
}
