#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

#include "bitfield.hh"

namespace gifproc::util {

bitfld<uint8_t> do_read8(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits);
bitfld<uint16_t> do_read16(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits);
bitfld<uint32_t> do_read32(std::vector<uint8_t> const& source, std::size_t off, std::size_t nbits);

void do_write8(std::vector<uint8_t>& sink, std::size_t off, bitfld<uint8_t> val);
void do_write16(std::vector<uint8_t>& sink, std::size_t off, bitfld<uint16_t> val);
void do_write32(std::vector<uint8_t>& sink, std::size_t off, bitfld<uint32_t> val);

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

template <typename T>
constexpr void do_write(std::vector<uint8_t>& sink, std::size_t off, bitfld<T> val) {
   if constexpr (sizeof(T) == sizeof(uint8_t)) {
      do_write8(sink, off, val);
   } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
      do_write16(sink, off, val);
   } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
      do_write32(sink, off, val);
   }
}

using streampos = typename std::make_signed_t<std::size_t>;

// Constant-bitwidth istream
template <std::size_t _Nbits>
class cbw_istream {
   static_assert(_Nbits > 0 && _Nbits <= bitsize_v<uint32_t>);
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

   stream_integral read_extract() {
      out_type result = read();
      return result._value;
   }

   cbw_istream<_Nbits>& operator>>(out_type& rhs) {
      rhs = read();
      return *this;
   }

   cbw_istream<_Nbits>& operator>>(stream_integral& rhs) {
      rhs = read_extract();
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

   constexpr streampos tell_index() const {
      return _pos / _Nbits_sp;
   }

   constexpr void seek_end() {
      _pos = static_cast<streampos>(_size);
   }

   constexpr bool eof() const {
      return _pos >= static_cast<streampos>(_size);
   }
};

// Constant-bitwidth istream
template <std::size_t _Nbits>
class cbw_ostream {
   static_assert(_Nbits > 0 && _Nbits <= bitsize_v<uint32_t>);
private:
   constexpr static streampos _Nbits_sp = static_cast<streampos>(_Nbits);

   std::vector<uint8_t>& _sink;
   streampos _cpos;

public:
   using stream_integral = typename smallest_uintegral<_Nbits>::type;
   using in_type = bitfld<stream_integral>;

   constexpr cbw_ostream(std::vector<uint8_t>& sink, std::size_t initial_size = 0)
         : _sink(sink), _cpos(initial_size) {}
   cbw_ostream(cbw_ostream&&) = delete;

   void write(in_type value) {
      do_write(_sink, static_cast<std::size_t>(_cpos), value);
      _cpos += value.mask_len();
   }

   void write(stream_integral value) {
      write(create_nbits(value, _Nbits));
   }

   cbw_ostream<_Nbits>& operator<<(in_type rhs) {
      write(rhs);
      return *this;
   }

   cbw_ostream<_Nbits>& operator<<(stream_integral rhs) {
      write(rhs);
      return *this;
   }

   void seek_to_index(std::size_t index) {
      const std::size_t bytes_to_i = (index / _Nbits) + 1;
      if (_sink.size() < bytes_to_i) {
         _sink.resize(bytes_to_i);
      }
      _cpos = index * _Nbits;
   }

   constexpr streampos tell_index() const {
      return _cpos / _Nbits;
   }

   constexpr std::size_t size() const {
      return static_cast<std::size_t>(_cpos);
   }
};

class vbw_istream {
private:
   std::vector<uint8_t> const& _source;
   const std::size_t _size;
   streampos _pos;

public:
   using stream_integral = uint32_t;
   using out_type = bitfld<stream_integral>;

   constexpr vbw_istream(std::vector<uint8_t> const& source, std::size_t size)
         : _source(source), _size(size), _pos(0) {}
   constexpr vbw_istream(vbw_istream&&) = delete;

   out_type read(std::size_t nbits) {
      assert(nbits <= bitsize_v<stream_integral>);
      if (eof()) {
         return out_type();
      }
      out_type ret = do_read<stream_integral>(_source, _pos, nbits);
      _pos += nbits;
      if (eof()) {
         ret = ret.trim_mask_right(_size - (_pos - nbits));
      }
      return ret;
   }

   stream_integral read_extract(std::size_t nbits) {
      out_type result = read(nbits);
      return result._value;
   }

   constexpr void rewind(streampos num_bits) {
      _pos = std::max(streampos{0}, _pos - num_bits);
   }

   constexpr void seek(streampos bit_idx) {
      _pos = std::max(streampos{0}, std::max(static_cast<streampos>(_size), bit_idx));
   }

   constexpr void seek_end() {
      _pos = static_cast<streampos>(_size);
   }

   constexpr bool eof() const {
      return _pos >= static_cast<streampos>(_size);
   }
};

class vbw_ostream {
private:
   std::vector<uint8_t>& _sink;
   streampos _cpos;

public:
   constexpr vbw_ostream(std::vector<uint8_t>& sink, std::size_t initial_size = 0)
         : _sink(sink), _cpos(static_cast<streampos>(initial_size)) {}

   template <typename T>
   void write(bitfld<T> value) {
      do_write(_sink, static_cast<std::size_t>(_cpos), value);
      _cpos += value.mask_len();
   }

   template <typename T>
   vbw_ostream& operator<<(bitfld<T> rhs) {
      write(rhs);
      return *this;
   }

   constexpr std::size_t size() const {
      return static_cast<std::size_t>(_cpos);
   }
};

}
