/**
 * @brief Ensures parsing is working correctly
*/

#include "../src/parse.hpp"
#include <cassert>
#include <iostream>

int main() {
    Parser p(lex_text("a in Set(a, b);", __FILE__));
    const ASTNode n = p.parse_expr();
    std::cout << n << '\n';
    assert(n == ASTNode("in", {ASTNode("a"), ASTNode("Set", {ASTNode("a"), ASTNode("b")})}));
    return 0;
}
