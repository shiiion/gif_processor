all: bin/test

bin/test: create_dir test.cc build/bitstream.o
	g++ -O0 -g test.cc build/bitstream.o -o bin/test

build/bitstream.o: bitstream.cc bitstream.hh bitfield.hh
	g++ -c bitstream.cc -o build/bitstream.o

create_dir:
	mkdir bin
	mkdir build

clean:
	rm -rf build/
	rm -rf bin/

.PHONY: create_dir clean
