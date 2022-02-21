#pragma once

#include <cstdint>
#include <vector>

#include "bitfield.hh"
#include "bitstream.hh"

namespace gifproc::lzw {

void lzw_compress_1bpp(util::cbw_istream<1>& in, util::vbw_ostream& out);
void lzw_compress_2bpp(util::cbw_istream<2>& in, util::vbw_ostream& out);
void lzw_compress_3bpp(util::cbw_istream<3>& in, util::vbw_ostream& out);
void lzw_compress_4bpp(util::cbw_istream<4>& in, util::vbw_ostream& out);
void lzw_compress_5bpp(util::cbw_istream<5>& in, util::vbw_ostream& out);
void lzw_compress_6bpp(util::cbw_istream<6>& in, util::vbw_ostream& out);
void lzw_compress_7bpp(util::cbw_istream<7>& in, util::vbw_ostream& out);
void lzw_compress_8bpp(util::cbw_istream<8>& in, util::vbw_ostream& out);

template <std::size_t _Bits>
void lzw_compress(util::cbw_istream<_Bits>& in, util::vbw_ostream& out) {
   if constexpr (_Bits == 1) {
      lzw_compress_1bpp(in, out);
   } else if constexpr (_Bits == 2) {
      lzw_compress_2bpp(in, out);
   } else if constexpr (_Bits == 3) {
      lzw_compress_3bpp(in, out);
   } else if constexpr (_Bits == 4) {
      lzw_compress_4bpp(in, out);
   } else if constexpr (_Bits == 5) {
      lzw_compress_5bpp(in, out);
   } else if constexpr (_Bits == 6) {
      lzw_compress_6bpp(in, out);
   } else if constexpr (_Bits == 7) {
      lzw_compress_7bpp(in, out);
   } else if constexpr (_Bits == 8) {
      lzw_compress_8bpp(in, out);
   }
}

// zero: Success, non-zero: Failure
enum class decompress_status {
   kSuccess = 0,

   // EOF was reached without reading an EOI code
   kUnexpectedEof,

   kMissingInitialClearCode,
   // Clear code is missing at the start of the data stream
   // Clear code is missing when codebook is full
   kDictionaryOverflow,

   // If a compression code is read that shouldn't be written out by a valid lzw compressor
   kInvalidCompressCode,
};

decompress_status lzw_decompress_1bpp(util::vbw_istream& in, util::cbw_ostream<1>& out);
decompress_status lzw_decompress_2bpp(util::vbw_istream& in, util::cbw_ostream<2>& out);
decompress_status lzw_decompress_3bpp(util::vbw_istream& in, util::cbw_ostream<3>& out);
decompress_status lzw_decompress_4bpp(util::vbw_istream& in, util::cbw_ostream<4>& out);
decompress_status lzw_decompress_5bpp(util::vbw_istream& in, util::cbw_ostream<5>& out);
decompress_status lzw_decompress_6bpp(util::vbw_istream& in, util::cbw_ostream<6>& out);
decompress_status lzw_decompress_7bpp(util::vbw_istream& in, util::cbw_ostream<7>& out);
decompress_status lzw_decompress_8bpp(util::vbw_istream& in, util::cbw_ostream<8>& out);

template <std::size_t _Bits>
decompress_status lzw_decompress(util::vbw_istream& in, util::cbw_ostream<_Bits>& out) {
   if constexpr (_Bits == 1) {
      return lzw_decompress_1bpp(in, out);
   } else if constexpr (_Bits == 2) {
      return lzw_decompress_2bpp(in, out);
   } else if constexpr (_Bits == 3) {
      return lzw_decompress_3bpp(in, out);
   } else if constexpr (_Bits == 4) {
      return lzw_decompress_4bpp(in, out);
   } else if constexpr (_Bits == 5) {
      return lzw_decompress_5bpp(in, out);
   } else if constexpr (_Bits == 6) {
      return lzw_decompress_6bpp(in, out);
   } else if constexpr (_Bits == 7) {
      return lzw_decompress_7bpp(in, out);
   } else if constexpr (_Bits == 8) {
      return lzw_decompress_8bpp(in, out);
   }
}

}
