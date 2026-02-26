#include "../src/parse.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

bool fail = false;

void compare(const std::vector<std::string> &_input,
             const ASTNode &_expected) {
  std::vector<Token> tokens;
  for (const auto &i : _input) {
    tokens.push_back(Token(i));
  }
  tokens.push_back(";");

  ASTNode obs;
  try {
    Parser p = Parser(TokenStream(tokens));
    // p.debug = true;
    obs = p.parse_expr();
  } catch (std::runtime_error &e) {
    fail = true;
    std::cout << "Error: " << e.what() << '\n';
    return;
  } catch (...) {
    fail = true;
    std::cout << "Unknown error\n";
    return;
  }

  if (obs != _expected) {
    std::cerr << "Expected\n"
              << _expected << "\nBut saw\n"
              << obs << "\n";
    fail = true;
  }
}

int main() {
  // clang-format off
  // Basic boolean logic
  compare(
    {"a", "and", "b", "or", "not", "c", "==", "true"},
    ASTNode(
      "and",
      {
        Token("a"),
        ASTNode(
          "or",
          {
            Token("b"),
            ASTNode(
              "not",
              {
                ASTNode(
                  "==",
                  {
                    Token("c"),
                    Token("true")
                  }
                )
              }
            )
          }
        )
      }
    )
  );

  compare(
    {"(", "a", "and", "b", "or", "not", "c", "==", "true", ")"},
    ASTNode(
      "and",
      {
        Token("a"),
        ASTNode(
          "or",
          {
            Token("b"),
            ASTNode(
              "not",
              {
                ASTNode(
                  "==",
                  {
                    Token("c"),
                    Token("true")
                  }
                )
              }
            )
          }
        )
      }
    )
  );

  // Boolean logic + fn calls
  compare(
    {"e", "(", "S", "(", "S", "(", "x", ")", ")", ")",
      "==", "e", "(", "x", ")"},
    ASTNode(
      "==",
      {
        ASTNode(
          "e",
          {
            ASTNode(
              "S",
              {
                ASTNode(
                  "S",
                  {
                    Token("x")
                  }
                )
              }
            )
          }
        ),
        ASTNode(
          "e",
          {
            Token("x")
          }
        )
      }
    )
  );

  // Boolean logic + fn calls + parentheses
  compare(
    {"not", "(",   "a", "(",       "b", ")",   "and", "b",
     "or",  "c",   ")", "implies", "(", "not", "a",   "iff",
     "(",   "not", "c", "and",     "b", ")",   ")"},
    ASTNode("implies",
      {
        ASTNode(
          "not",
          {
            ASTNode(
              "and",
              {
                ASTNode(
                  "a",
                  {
                    Token("b")
                  }
                ),
                ASTNode(
                  "or",
                  {
                    Token("b"),
                    Token("c")
                  }
                )
              }
            )
          }
        ),
        ASTNode(
          "iff",
          {
            ASTNode(
              "not",
              {
                Token("a")
              }
            ),
            ASTNode(
              "and",
              {
                ASTNode(
                  "not",
                  {
                    Token("c")
                  }
                ),
                Token("b")
              }
            )
          }
        )
      }
    )
  );

  // Quantification and domain
  compare(
    {"forall", "x", ".", "x", "in", "Megaset"},
    ASTNode(
      "forall",
      {
        Token("x"),
        ASTNode(
          "in",
          {
            Token("x"),
            Token("Megaset")
          }
        )
      }
    )
  );
  compare(
    {"forall", "x", "in", "Megaset", ".", "phi", "(", "x", ")"},
    ASTNode(
      "forall",
      {
        ASTNode(
          "in",
          {
            Token("x"),
            Token("Megaset")
          }
        ),
        ASTNode(
          "phi",
          {
            Token("x")
          }
        )
      }
    )
  );

  // clang-format on

  if (fail) {
    return 1;
  }
  return 0;
}
