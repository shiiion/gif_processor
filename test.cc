#include <cstdio>

#include "bitstream.hh"
#include "bitfield.hh"

void test_cbw_istream() {
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

void test_vbw_iostreams() {
   using gifproc::util::bitfld;
   using gifproc::util::create_nbits;
   std::vector<uint8_t> sample;
   gifproc::util::vbw_ostream vbwo(sample);

   vbwo << create_nbits<uint8_t>(5, 3);
   vbwo << create_nbits<uint8_t>(6, 3);
   vbwo << create_nbits<uint8_t>(7, 3);
   vbwo << create_nbits<uint8_t>(7, 4);
   vbwo << create_nbits<uint8_t>(8, 4);
   vbwo << create_nbits<uint8_t>(0, 4);
   vbwo << create_nbits<uint8_t>(62, 6);

   gifproc::util::vbw_istream vbwi(sample, vbwo.size());
   bitfld<uint32_t> val;
   val = vbwi.read(3);
   printf("val: %x  nbits: %d\n", val._value, val.mask_len());
   val = vbwi.read(3);
   printf("val: %x  nbits: %d\n", val._value, val.mask_len());
   val = vbwi.read(3);
   printf("val: %x  nbits: %d\n", val._value, val.mask_len());
   val = vbwi.read(4);
   printf("val: %x  nbits: %d\n", val._value, val.mask_len());
   val = vbwi.read(4);
   printf("val: %x  nbits: %d\n", val._value, val.mask_len());
   val = vbwi.read(4);
   printf("val: %x  nbits: %d\n", val._value, val.mask_len());
   val = vbwi.read(6);
   printf("val: %x  nbits: %d\n", val._value, val.mask_len());
}

int main() {
   test_vbw_iostreams();
}
