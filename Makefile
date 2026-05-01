.PHONY:	clean docs format all test

CPP = g++ -pedantic -Wall -std=c++20 -O3 -g
HEADERS = src/parse.hpp src/inference.hpp src/core.hpp \
	src/congruence.hpp src/cdcl.hpp
TESTS = tests/expr_parse_test.out tests/parse_verily.out \
	tests/pattern_matching.out

OBJECTS = $(HEADERS:.hpp=.o)

all:	verily.out

test:	tests/expr_parse_test.out tests/parse_verily.out \
	tests/pattern_matching.out
	./tests/pattern_matching.out

%.out:	%.o $(OBJECTS)
	$(CPP) -o $@ $^

%.o:	%.cpp $(HEADERS)
	$(CPP) -c -o $@ $<

format:
	find . -type f \( -iname "*.cpp" -or -iname "*.hpp" \) \
		-exec clang-format -i "{}" \;

clean:
	find . \( -iname "*.o" -or -iname "*.out" -or -iname \
		"*.verily.*" -or -iname "*.log" \) -exec rm -rf "{}" \;
	rm -rf latex/

docs:
	doxygen -q
	$(MAKE) -C latex
	cp latex/refman.pdf refman.pdf
