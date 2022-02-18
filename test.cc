#include <cstdio>

#include "bitstream.hh"
#include "bitfield.hh"

int main() {
   std::vector<uint8_t> sample;
   sample.push_back(0x14); // 0001 0100
   sample.push_back(0x68); // 0110 1000
   sample.push_back(0xff); // 1111 1111
   gifproc::util::cbw_istream<10> stream(sample, 24);

   while (!stream.eof()) {
      gifproc::util::cbw_istream<10>::out_type idx;
      stream >> idx;
      printf("val: %x  nbits: %d\n", idx._value, idx.mask_len());
   }
   stream.rewind(4);
   stream.seek(9);
   stream.rewind(3);
   while (!stream.eof()) {
      gifproc::util::cbw_istream<10>::out_type idx;
      stream >> idx;
      printf("val: %x  nbits: %d\n", idx._value, idx.mask_len());
   }
}
