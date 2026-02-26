/*
Tests pattern matching in the verily src code
*/

#include "../src/inference.hpp"
#include "../src/parse.hpp"
#include <cassert>

int main() {
  std::set<ASTNode> free_variables = {ASTNode("f"),
                                      ASTNode("x")};
  std::list<std::pair<ASTNode, ASTNode>> replacements;

  assert(
      InferenceMaker::is_of_form(ASTNode("a", {ASTNode("b")}),
                                 ASTNode("f", {ASTNode("x")}),
                                 free_variables, replacements));

  // This should pass w/ a = f, b = f(x), c = x
  free_variables = {ASTNode("a"), ASTNode("b"), ASTNode("c")};
  replacements.clear();
  assert(InferenceMaker::is_of_form(
      // to_examine
      ASTNode("==",
              {ASTNode("f", {ASTNode("f", {ASTNode("x")})}),
               ASTNode("x")}),
      // form
      ASTNode("==",
              {ASTNode("a", {ASTNode("b")}), ASTNode("c")}),
      // other junk
      free_variables, replacements));

  return 0;
}
