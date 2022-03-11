CXX := g++
CXXFLAGS_DEBUG := -O0 -g
CXXFLAGS_RELEASE := -O2
CXXFLAGS := $(CXXFLAGS_DEBUG) -Wall -MD -MP --std=c++17

SRC = $(wildcard *.cc)

all: bin build bin/test

bin/test: $(SRC:%.cc=build/%.o)
	$(CXX) -limagequant -o $@ $^

build/%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@


build:
	mkdir build

bin:
	mkdir bin

clean:
	rm -rf build/
	rm -rf bin/

-include $(SRC:%.cc=build/%.d)

.PHONY: clean
