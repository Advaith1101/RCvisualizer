CXX      = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

all: simulator detector tracer

simulator: simulator.cpp
	$(CXX) $(CXXFLAGS) -o simulator simulator.cpp

detector: detector.cpp
	$(CXX) $(CXXFLAGS) -o detector detector.cpp

tracer: tracer.cpp
	$(CXX) $(CXXFLAGS) -o tracer tracer.cpp

clean:
	rm -f simulator detector tracer trace.log shared_resource.txt

# Demo runs
demo-unsafe:
	@echo "=== Running UNSAFE (no sync) ==="
	./simulator 4 200 0

demo-mutex:
	@echo "=== Running with MUTEX ==="
	./simulator 4 200 1

demo-semaphore:
	@echo "=== Running with SEMAPHORE ==="
	./simulator 4 200 2

demo-trace:
	./tracer trace.log

.PHONY: all clean demo-unsafe demo-mutex demo-semaphore demo-trace
