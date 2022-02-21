all: bin/test

bin/test: bin build test.cc build/bitstream.o build/lzw.o
	g++ -O2 test.cc build/bitstream.o build/lzw.o -o bin/test

build/lzw.o: lzw.cc lzw.hh bitfield.hh bitstream.hh
	g++ -O2 -c lzw.cc -o build/lzw.o

build/bitstream.o: bitstream.cc bitstream.hh bitfield.hh
	g++ -O2 -c bitstream.cc -o build/bitstream.o

build:
	mkdir build

bin:
	mkdir bin

clean:
	rm -rf build/
	rm -rf bin/

.PHONY: clean
