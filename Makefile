.PHONY:	clean docs format all

CPP = g++ -pedantic -Wall -std=c++20 -O3 -g
HEADERS = src/parse.hpp src/inference.hpp src/core.hpp
TESTS = tests/expr_parse_test.out tests/parse_verily.out \
	tests/pattern_matching.out

OBJECTS = $(HEADERS:.hpp=.o)

all:	verily.out

%.out:	%.o $(OBJECTS)
	$(CPP) -o $@ $^

%.o:	%.cpp $(HEADERS)
	$(CPP) -c -o $@ $<

format:
	find . -type f \( -iname "*.cpp" -or -iname "*.hpp" \) \
		-exec clang-format -i "{}" \;

clean:
	find . \( -type f -iname "*.o" -or -iname "*.out" \) \
		-exec rm -rf "{}" \;

docs:
	doxygen -q
	$(MAKE) -C latex
	cp latex/refman.pdf refman.pdf
