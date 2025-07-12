CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2

TARGET := ray-scene-tracer
SRC := $(sort $(wildcard src/*.cpp))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

test: $(TARGET)
	./$(TARGET) --help >/dev/null

clean:
	rm -f $(TARGET)
