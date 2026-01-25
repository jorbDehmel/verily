CPP = g++ -pedantic -Wall -std=c++20 -O3 -g
HEADERS = src/parse.hpp src/inference.hpp
TESTS = tests/expr_parse_test.out tests/parse_verily.out

OBJECTS = $(HEADERS:.hpp=.o)

verily.out:	verily.o src/parse.o src/inference.o
	$(CPP) -o $@ $^

%.out:	%.o $(OBJECTS)
	$(CPP) -o $@ $^

%.o:	%.cpp $(HEADERS)
	$(CPP) -c -o $@ $<

.PHONY:	clean docs

clean:
	find . \( -type f -iname "*.o" -or -iname "*.out" \) \
		-exec rm -rf "{}" \;

docs:
	doxygen -q
	$(MAKE) -C latex
	cp latex/refman.pdf refman.pdf
