#include <cstdio>
#include <ctime>
#include <random>

#include "bitfield.hh"
#include "bitstream.hh"
#include "dequantize.hh"
#include "gif_processor.hh"
#include "lzw.hh"
#include "piximg.hh"
#include "quantize.hh"

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
   for (int i = 0; i < 512 * 512; i++) {
      initial_stream << static_cast<uint8_t>(random_dist(engine));
   }

   double start = static_cast<double>(clock()) / CLOCKS_PER_SEC;
   gifproc::util::cbw_istream<8> raw_stream_in(raw_data, initial_stream.size());
   gifproc::util::vbw_ostream compress_stream_out(compressed_data);
   gifproc::lzw::lzw_compress(raw_stream_in, compress_stream_out);

   gifproc::util::vbw_istream compress_stream_in(compressed_data, compress_stream_out.size());
   gifproc::util::cbw_ostream<8> decompress_stream_out(decompressed_data);
   gifproc::lzw::decompress_status status = gifproc::lzw::lzw_decompress(compress_stream_in, decompress_stream_out);
   double end = static_cast<double>(clock()) / CLOCKS_PER_SEC;

   assert(status == gifproc::lzw::decompress_status::kSuccess);

   double compress_ratio = static_cast<double>(compress_stream_out.size()) / initial_stream.size();
   printf("Deflation ratio for %ld bit data: %f (%ld raw %ld compressed) completed in %f seconds\n",
          _Bits,
          compress_ratio,
          initial_stream.size(),
          compress_stream_out.size(),
          end - start);

   for (std::size_t i = 0; i < raw_data.size(); i++) {
      assert(raw_data[i] == decompressed_data[i]);
   }
}

void test_make_funny(const char* path, int thickness, int range_b, int range_e) {
   gifproc::gif test_gif;
   auto read_result = test_gif.open_read(path);
   if (read_result != gifproc::gif_parse_result::kSuccess) {
      return;
   }
   auto mq_ctx = gifproc::quant::begin_quantize_multiple(test_gif.width(), test_gif.height());
   int ctr = 0;
   int size = static_cast<int>(test_gif.nframes());
   range_b = (range_b % size + size) % size;
   range_e = (range_e % size + size) % size;
   if (range_b > range_e) {
      return;
   }
   test_gif.foreach_frame([&mq_ctx, thickness, &ctr, range_b, range_e] (gifproc::quant::gif_frame const& img, gifproc::gif_frame_context const& ctx,
                                     std::vector<gifproc::color_table_entry> const& gct) {
         if (ctr % 1 == 0) {
            if (ctr >= range_b && ctr <= range_e) {
               gifproc::piximg pimg(img);
               pimg.dump_to("frame.raw");
               //pimg.add_speech_bubble_to_top(thickness);
               if (ctx._extension) {
                  gifproc::quant::step_quantize_multiple(pimg, mq_ctx, ctx._extension->_delay_time * 1);
               } else {
                  gifproc::quant::step_quantize_multiple(pimg, mq_ctx, 0);
               }
            }
         }
         ctr++;
      });
   gifproc::quant::end_quantize_multiple(mq_ctx);

   gifproc::gif out_gif;
   out_gif.open_write("out.gif");
   gifproc::quant::foreach_quantize_multi(mq_ctx, [&out_gif] (gifproc::quant::qimg const& img, uint16_t old_delay) {
         out_gif.add_frame(img, old_delay);
      });
   out_gif.finish_write(mq_ctx._palette);
}

int main(int argc, char** argv) {
   if (argc == 2) {
      test_make_funny(argv[1], 6, 0, -1);
   } else if (argc == 3) {
      int val;
      val = strtol(argv[2], nullptr, 10);
      test_make_funny(argv[1], val, 0, -1);
   } else if (argc == 5) {
      int val, val2, val3;
      val = strtol(argv[2], nullptr, 10);
      val2 = strtol(argv[3], nullptr, 10);
      val3 = strtol(argv[4], nullptr, 10);
      test_make_funny(argv[1], val, val2, val3);
   } else {
      printf("Invalid argument count");
   }
   return 0;
}
