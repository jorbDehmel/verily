/**
 * @brief Hoare logic
 */

#pragma once
#include "ast.hpp"

/**
 * @brief Given some precondition and some statement, returns
 * the minimal postcondition. The statements can be of form
 * {'_', 'SET', 'ITE', 'WHILE'} having arity of *, 2, 3, and 1,
 * respectively. This follows standard Hoare logic rules, but
 * only works its way from the start to the end (never using
 * information from the postcondition until the final entailment
 * check). The precondition is some arbitrary AST, although the
 * returned formula will use the Boolean connectives 'and'
 * (arity 2) and 'not' (arity 1).
 */
ASTNode get_postcondition(const ASTNode &_precondition,
                          const ASTNode &_statement) noexcept;
