#include "../src/parse.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    const auto tree = Parser(lex_file(argv[i])).parse();
    std::cout << "File " << argv[i] << ":\n";
    fancy_print(std::cout, tree);
    std::cout << "\n\n";
  }
  return 0;
}
