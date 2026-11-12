CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O1 -I third_party
SRC      = src/server.cpp src/api.cpp src/serializer.cpp src/json_mini.cpp src/process.cpp src/stats.cpp src/scheduler.cpp

server: $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ -pthread

clean:
	rm -f server

.PHONY: clean
