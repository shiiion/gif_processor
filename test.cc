#include <cstdio>
#include <ctime>
#include <random>

#include "bitstream.hh"
#include "bitfield.hh"
#include "lzw.hh"

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

template <std::size_t _Bits>
void test_lzw_random_compress() {
   std::random_device r;
   std::default_random_engine engine(r());
   std::uniform_int_distribution<int> random_dist(0, (1 << _Bits) - 1);

   std::vector<uint8_t> raw_data, compressed_data, decompressed_data;
   gifproc::util::cbw_ostream<8> initial_stream(raw_data);
   for (int i = 0; i < 4096 * 4096; i++) {
      initial_stream << static_cast<uint8_t>(random_dist(engine));
   }

   double start = static_cast<double>(clock()) / CLOCKS_PER_SEC;
   gifproc::util::cbw_istream<8> raw_stream_in(raw_data, initial_stream.size());
   gifproc::util::vbw_ostream compress_stream_out(compressed_data);
   gifproc::lzw::lzw_compress(raw_stream_in, compress_stream_out);

   gifproc::util::vbw_istream compress_stream_in(compressed_data, compress_stream_out.size());
   gifproc::util::cbw_ostream<8> decompress_stream_out(decompressed_data);
   gifproc::lzw::lzw_decompress(compress_stream_in, decompress_stream_out);
   double end = static_cast<double>(clock()) / CLOCKS_PER_SEC;

   double compress_ratio = static_cast<double>(compress_stream_out.size()) / initial_stream.size();
   printf("Deflation ratio for %d bit data: %f (%d raw %d compressed) completed in %f seconds\n",
          _Bits,
          compress_ratio,
          initial_stream.size(),
          compress_stream_out.size(),
          end - start);

   for (int i = 0; i < raw_data.size(); i++) {
      assert(raw_data[i] == decompressed_data[i]);
   }
}

int main() {
   test_lzw_random_compress<2>();
   test_lzw_random_compress<3>();
   test_lzw_random_compress<4>();
   test_lzw_random_compress<5>();
   test_lzw_random_compress<6>();
   test_lzw_random_compress<7>();
   test_lzw_random_compress<8>();
}
