# Compiler settings - Can change to clang++ if preferred
CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread

# Build targets
TARGET = threadpool
# TARGET = tptest
# TARGET = tpcustomtest
SRC = thread-pool.cc main.cc Semaphore.cc
# SRC = thread-pool.cc tptest.cc Semaphore.cc
# SRC = thread-pool.cc tpcustomtest.cc Semaphore.cc


# Link the target with object files
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Clean up build artifacts
clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
