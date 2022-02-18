#pragma once

#include <cstdint>
#include <vector>

#include "bitfield.hh"

namespace gifproc::util {

bitfld<uint8_t> do_read8(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits);
bitfld<uint16_t> do_read16(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits);
bitfld<uint32_t> do_read32(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits);

template <typename T>
constexpr bitfld<T> do_read(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits) {
   if constexpr (sizeof(T) == sizeof(uint8_t)) {
      return do_read8(source, off, nbits);
   } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
      return do_read16(source, off, nbits);
   } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
      return do_read32(source, off, nbits);
   }
}

using streampos = typename std::make_signed_t<std::size_t>;

// Constant-bitwidth istream
template <std::size_t _Nbits>
class cbw_istream {
private:
   constexpr static streampos _Nbits_sp = static_cast<streampos>(_Nbits);

   std::vector<uint8_t> const& _source;
   const std::size_t _size;
   streampos _pos;

public:
   using stream_integral = typename smallest_uintegral<_Nbits>::type;
   using out_type = bitfld<stream_integral>;

   constexpr cbw_istream(std::vector<uint8_t> const& source, std::size_t size)
         : _source(source), _size(size), _pos(0) {}
   cbw_istream(cbw_istream&&) = delete;

   out_type read() {
      if (eof()) {
         return out_type();
      }
      out_type ret = do_read<stream_integral>(_source, _pos, _Nbits);
      _pos += _Nbits_sp;
      if (eof()) {
         ret = ret.trim_mask_right(_size - (_pos - _Nbits));
      }
      return ret;
   }

   cbw_istream& operator>>(out_type& rhs) {
      rhs = read();
      return *this;
   }

   constexpr void rewind(streampos num_rewinds) {
      _pos = std::max(streampos{0}, _pos - _Nbits_sp * num_rewinds);
   }

   constexpr void seek(streampos loc) {
      _pos = std::max(streampos{0},
             std::min(_Nbits_sp * loc,
                      // Round up to the next position after eof to maintain alignment
                      ((static_cast<streampos>(_size) + _Nbits_sp - 1) / _Nbits_sp) * _Nbits_sp));
   }

   constexpr bool eof() const {
      return _pos >= _size;
   }
};

}
