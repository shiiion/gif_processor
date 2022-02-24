CXXFLAGS_DEBUG := -O0 -g
CXXFLAGS_RELEASE = -O2
CXXFLAGS = $(CXXFLAGS_DEBUG) -Wall -Werror


all: bin/test

bin/test: bin build test.cc build/bitstream.o build/lzw.o build/gif_processor.o build/quantize.o
	g++ $(CXXFLAGS) test.cc build/bitstream.o build/lzw.o build/gif_processor.o build/quantize.o -o bin/test

build/lzw.o: lzw.cc lzw.hh bitfield.hh bitstream.hh
	g++ $(CXXFLAGS) -c lzw.cc -o build/lzw.o

build/gif_processor.o: gif_processor.cc gif_processor.hh bitstream.hh bitfield.hh lzw.hh gif_spec.hh
	g++ $(CXXFLAGS) -c gif_processor.cc -o build/gif_processor.o

build/bitstream.o: bitstream.cc bitstream.hh bitfield.hh
	g++ $(CXXFLAGS) -c bitstream.cc -o build/bitstream.o

build/quantize.o: quantize.cc quantize.hh bitstream.hh bitfield.hh gif_spec.hh
	g++ $(CXXFLAGS) -c quantize.cc -o build/quantize.o

build:
	mkdir build

bin:
	mkdir bin

clean:
	rm -rf build/
	rm -rf bin/

.PHONY: clean
