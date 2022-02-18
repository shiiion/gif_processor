#include "bitstream.hh"

#include <cassert>
#include <cstring>

namespace gifproc::util {

template <typename _IntType>
bitfld<_IntType> do_read_generic(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits) {
   // Allow partial reads, but disallow starting after the source
   assert(off < to_bit(source.size()));

   using fit_type = typename smallest_uintegral<bitsize_v<_IntType> + 1>::type;
   const std::size_t fbit = off, lbit = off + nbits - 1;
   const std::size_t fbyte = to_byte(fbit);
   const std::size_t fbit_a = bit_align(fbit);
   const std::size_t fbit_rel = fbit - fbit_a, lbit_rel = lbit - fbit_a;

   // Clamped
   const std::size_t lbit_c = std::min(lbit, to_bit(source.size()) - 1);
   const std::size_t lbyte_c = to_byte(lbit_c);

   fit_type source_packed = fit_type{0};
   if (is_little_endian()) {
      uint8_t* source_packed_bytes = reinterpret_cast<uint8_t*>(&source_packed);
      for (std::size_t i = 0; i < lbyte_c - fbyte + 1; i++) {
         source_packed_bytes[sizeof(source_packed) - 1 - i] = source[fbyte + i];
      }
   } else if (is_big_endian()) {
      memcpy(&source_packed, &source[fbyte], lbyte_c - fbyte + 1);
   }
   // Use normal bitmask range [fbit, lbit], remainder of source_packed should be filled with 0
   const bitfld<fit_type> packed_bits = bitfld<fit_type>(source_packed, fbit_rel, lbit_rel);
   const bitfld<fit_type> extract_bits = packed_bits.extract_to_lsb();
   return bitfld<_IntType>(static_cast<_IntType>(extract_bits._value), static_cast<_IntType>(extract_bits._mask));
}

bitfld<uint8_t> do_read8(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits) {
   assert(nbits <= 8);
   return do_read_generic<uint8_t>(source, off, nbits);
}

bitfld<uint16_t> do_read16(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits) {
   assert(nbits <= 16);
   return do_read_generic<uint16_t>(source, off, nbits);
}

bitfld<uint32_t> do_read32(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits) {
   assert(nbits <= 32);
   return do_read_generic<uint32_t>(source, off, nbits);
}

}
