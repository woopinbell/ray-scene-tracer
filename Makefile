CXX ?= c++
CPPFLAGS ?= -Iinclude
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2

TARGET := ray-scene-tracer
SRC := $(sort $(wildcard src/*.cpp))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(SRC)

test: $(TARGET)
	tests/render_smoke.sh

clean:
	rm -f $(TARGET)
