#include "src/ast.hpp"
#include "src/parse.hpp"
#include <cassert>
#include <iostream>

void test_subs() {
  Parser p;
  p.order_of_operations.push_back(
      {"subs", InferenceRule(
                   {ASTNode("a"), ASTNode("b"), ASTNode("c")},
                   {
                       ASTNode("a"),
                       ASTNode("["),
                       ASTNode("b"),
                       ASTNode("="),
                       ASTNode("c"),
                       ASTNode("]"),
                   },
                   ASTNode("subs", {ASTNode("a"), ASTNode("b"),
                                    ASTNode("c")}))});
  p.order_of_operations.push_back(
      {"lambda",
       InferenceRule(
           {ASTNode("x"), ASTNode("M")},
           {
               ASTNode("lambda"),
               ASTNode("x"),
               ASTNode("."),
               ASTNode("M"),
           },
           ASTNode("lambda", {ASTNode("x"), ASTNode("M")}))});

  std::cout << "OOO:\n";
  for (const auto &pair : p.order_of_operations) {
    std::cout << pair.first << ": " << pair.second << '\n';
  }

  p.ts.append(
      lex_text("(lambda x . M)[y = N];", __FILE__).data);
  auto res = p.parse_expr();
  std::cout << "Got " << res << '\n';

  p.ts.append(
      lex_text("axiom lambda_calc_subs: over x, y, M, N\n"
               "given (lambda x. M)[y = N]\n"
               "deduce lambda x. (M[y = N]);",
               __FILE__)
          .data);
  res = p.parse();
  std::cout << "Parsed " << res << '\n';
}

int main() {
  test_subs();
}
