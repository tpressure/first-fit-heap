CXXFLAGS = -I../include -std=c++11 -O2 -Wall -Wextra -Werror -DHEAP_ENABLE_ASSERT -DHEAP_LINUX
SANITIZER_FLAGS = -fsanitize=address -fsanitize=undefined
default: heap_test

heap_test: main.cpp
	$(CXX) $(CXXFLAGS) $(SANITIZER_FLAGS) -o $@ $<

heap_test_travis: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf heap_test
