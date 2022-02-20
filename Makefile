all: bin/test bin/lzw

bin/test: bin build test.cc build/bitstream.o
	g++ -O0 -g test.cc build/bitstream.o -o bin/test

bin/lzw: bin build lzw.cc build/bitstream.o
	g++ -O0 -g lzw.cc build/bitstream.o -o bin/lzw

build/bitstream.o: bitstream.cc bitstream.hh bitfield.hh
	g++ -O0 -g -c bitstream.cc -o build/bitstream.o

build:
	mkdir build

bin:
	mkdir bin

clean:
	rm -rf build/
	rm -rf bin/

.PHONY: clean
