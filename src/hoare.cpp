#include "hoare.hpp"

ASTNode get_postcondition(const ASTNode &_precondition,
                          const ASTNode &_statement) noexcept {
  if (_statement.text == "_") {
    if (_statement.children.size() == 0) {
      return _precondition;
    } else {
      ASTNode Q = _precondition;
      for (const auto &stmt : _statement.children) {
        Q = get_postcondition(Q, stmt);
      }
      return Q;
    }
  } else if (_statement.text == "SET") {
    if (_statement.children.size() != 2) {
      return ASTNode("malformed_hoare_statement", {_statement});
    }
    const auto what = _statement.children.at(0); // Variable
    const auto with = _statement.children.at(1); // Value
    return _precondition.replace(with, what);    // Intentional!
  } else if (_statement.text == "ITE") {
    if (_statement.children.size() != 3) {
      return ASTNode("malformed_hoare_statement", {_statement});
    }
    const auto i = _statement.children.at(0);
    const auto t = _statement.children.at(1);
    const auto e = _statement.children.at(2);
    return ASTNode(
        "or",
        {get_postcondition(ASTNode("and", {i, _precondition}),
                           t),
         get_postcondition(ASTNode("and", {ASTNode("not", {i}),
                                           _precondition}),
                           e)});
  } else if (_statement.text == "WHILE") {
    if (_statement.children.size() != 2) {
      return ASTNode("malformed_hoare_statement", {_statement});
    }
    const auto c = _statement.children.at(0);
    const auto b = _statement.children.at(1);
    return ASTNode(
        "and", {ASTNode("not", {c}),
                get_postcondition(
                    ASTNode("and", {c, _precondition}), b)});
  }
  return ASTNode("get_postcondition_error",
                 {_precondition, _statement});
}
