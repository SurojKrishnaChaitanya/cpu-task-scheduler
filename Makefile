CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O1 -I third_party
CORE     = src/process.cpp src/stats.cpp src/scheduler.cpp
API      = src/api.cpp src/serializer.cpp src/json_mini.cpp

all: tests api_tests server

tests: src/tests.cpp $(CORE)
	$(CXX) $(CXXFLAGS) -o $@ $^

api_tests: src/api_tests.cpp $(API) $(CORE)
	$(CXX) $(CXXFLAGS) -o $@ $^

server: src/server.cpp $(API) $(CORE)
	$(CXX) $(CXXFLAGS) -o $@ $^ -pthread

check: tests api_tests
	./tests && ./api_tests

clean:
	rm -f tests api_tests server

.PHONY: all check clean
