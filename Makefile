.PHONY:	clean docs format all test

CPP = g++ -pedantic -Wall -std=c++20 -O3 -g
HEADERS = src/parse.hpp src/inference.hpp src/core.hpp \
	src/ast.hpp src/packages.hpp

OBJECTS = $(HEADERS:.hpp=.o)

all:	$(OBJECTS) verily.out hark.out

install:	verily.out hark.out
	echo 'y' | ./hark.out --purge
	./hark.out --add-local-version .
	./hark.out --add-local std

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
