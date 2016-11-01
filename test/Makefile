CXXFLAGS = -I../include -std=c++14 -O2 -Wall -Wextra -Werror

default: heap_test

heap_test: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf heap_test
