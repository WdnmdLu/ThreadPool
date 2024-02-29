CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -pedantic -pthread

output: main.cpp threadPool.cpp
	touch threadPool.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f output
