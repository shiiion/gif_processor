#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <optional>

namespace lzw {
namespace {

constexpr std::size_t to_byte(std::size_t bit) { return bit >> 3; }
constexpr std::size_t bit_align(std::size_t bit) { return bit & ~7; }

struct bitfield_value {
   uint16_t _value;
   uint8_t _bit_size;

   constexpr bitfield_value(uint16_t val, uint8_t bits) : _value(val), _bit_size(bits) {}
};

// LSB ordered bitstream
class lsb_istream {
private:
   std::vector<uint8_t> const& _data_source;
   std::size_t _stream_pos;
   std::size_t _stream_size;

public:
   constexpr lsb_istream(std::vector<uint8_t> const& data_source, std::size_t stream_size)
         : _data_source(data_source), _stream_pos(0), _stream_size(stream_size) {}

   uint16_t read_some(uint8_t bit_size) {
      uint16_t ret = read_at(_stream_pos, bit_size);
      _stream_pos += bit_size;
      return ret;
   }

   void rewind(uint8_t bit_size) {
      _stream_pos = std::max(_stream_pos - bit_size, static_cast<std::size_t>(0));
   }

   constexpr bool eof() const {
      return _stream_pos >= _stream_size;
   }

   uint16_t read_at(std::size_t bit_begin, uint8_t bit_size) const {
      assert(bit_size <= 12);
      if (bit_begin >= _stream_size) {
         return 0;
      }

      uint16_t value_out = 0;
      for (uint8_t i = 0; i < bit_size; i++) {
         value_out <<= 1;
         const std::size_t cur_bit = bit_begin + i;
         if (cur_bit < _stream_size) {
            const std::size_t cur_byte = to_byte(cur_bit);
            const std::size_t cur_bit_align = bit_align(cur_bit);
            const std::size_t cur_bit_shift = 7 - (cur_bit - cur_bit_align);

            value_out |= (_data_source[cur_byte] >> cur_bit_shift) & 1;
         }
      }
      return value_out;
   }
};

class lsb_ostream {
private:
   std::vector<uint8_t>& _data_sink;
   std::size_t _stream_pos;
   std::size_t _stream_size;

public:
   constexpr lsb_ostream(std::vector<uint8_t>& data_sink, std::size_t init_stream_size)
         : _data_sink(data_sink), _stream_pos(0), _stream_size(init_stream_size) {}

   constexpr std::size_t stream_size() const {
      return _stream_size;
   }

   void append_some(bitfield_value value) {
      append_some(value._value, value._bit_size);
   }

   void append_some(uint16_t value, uint8_t bit_size) {
      write_at(_stream_pos, value, bit_size);
      _stream_pos += bit_size;
   }

   void write_at(std::size_t bit_begin, bitfield_value value) {
      write_at(bit_begin, value._value, value._bit_size);
   }

   void write_at(std::size_t bit_begin, uint16_t value, uint8_t bit_size) {
      assert(bit_size <= 12);
      _stream_size = std::max(_stream_size, bit_begin + bit_size);
      while (to_byte(bit_begin + bit_size) >= _data_sink.size()) {
         _data_sink.push_back(0);
      }
      for (uint8_t i = 0; i < bit_size; i++) {
         const std::size_t cur_bit = bit_begin + i;
         const std::size_t cur_byte = to_byte(cur_bit);
         const std::size_t cur_bit_align = bit_align(cur_bit);
         const std::size_t cur_bit_shift = 7 - (cur_bit - cur_bit_align);
         const uint8_t or_mask = ((value >> (bit_size - i - 1)) & 1) << cur_bit_shift;
         _data_sink[cur_byte] = (_data_sink[cur_byte] & ~or_mask) | or_mask;
      }
   }
};

using codebook_reference = uint16_t;

struct lookup_result {
   constexpr static uint16_t kEOFUnit = 0xffff;
   bitfield_value _output;
   codebook_reference _entry;
   uint16_t _miss;

   constexpr lookup_result(bitfield_value output, codebook_reference entry, uint16_t miss)
         : _output(output), _entry(entry), _miss(miss) {}
};

template <std::size_t _bpp>
class lzw_codebook {
private:
   // codebook_entry defines a single node within the codebook trie
   struct codebook_entry {
      constexpr static uint16_t kInvalidConnection = 0xffff;

      std::array<uint16_t, 1 << _bpp> _connections;
      uint16_t _codebook_value;
      uint8_t _table_key;

      // Allow uninitialized data! Only initialize upon "allocation"

      void initialize(uint16_t value = 0, uint8_t key = 0) {
         _connections.fill(kInvalidConnection);
         _codebook_value = value;
         _table_key = key;
      }
   };

   constexpr static uint16_t kMaxCodebookEntry = (1 << 12) - 1;

   codebook_entry _codebook_head;
   std::array<codebook_entry, kMaxCodebookEntry> _codebook_table;
   uint16_t _codebook_size;

   constexpr uint16_t clear_code() const {
      return 1 << _bpp;
   }

   constexpr uint16_t eoi_code() const {
      return clear_code() + 1;
   }

   constexpr uint8_t get_bitsize() const {
      // The number of bits for variable lzw is dependant on the highest (currently) generatable value:
      //    32 - cntlzw(_codebook_size)
      // This will always be at least one greater than the _bpp
      return 32 - __builtin_clz(static_cast<unsigned int>(_codebook_size - 1));
   }

   lzw_codebook() : _codebook_size(eoi_code() + 1) {
      _codebook_head.initialize();
      for (uint16_t i = 0; i < _codebook_size; i++) {
         _codebook_table[i].initialize(i, i);
         _codebook_head._connections[i] = i;
      }
   }

public:
   static std::unique_ptr<lzw_codebook<_bpp>> alloc_codebook() {
      return std::unique_ptr<lzw_codebook>(new lzw_codebook<_bpp>());
   }

   constexpr bitfield_value clear_code_now() const {
      return bitfield_value(clear_code(), get_bitsize());
   }
   // Does the operation of looking up an entry, breaking at the first miss
   // Moves the bitstream position to the location of the miss
   lookup_result lookup_phase_1(lsb_istream& data_stream) const {
      uint16_t unit = data_stream.read_some(_bpp);
      uint16_t table_index = codebook_entry::kInvalidConnection;

      codebook_entry const* it = &_codebook_head;
      while (it->_connections[unit] != codebook_entry::kInvalidConnection &&
             !data_stream.eof()) {
         it = &_codebook_table[table_index = it->_connections[unit]];
         unit = data_stream.read_some(_bpp);
      }
      // If we reach EOF and the last unit makes up a fully mapped sequence
      if (it->_connections[unit] != codebook_entry::kInvalidConnection) {
         const uint16_t final_index = it->_connections[unit];
         return lookup_result(bitfield_value(_codebook_table[final_index]._codebook_value, get_bitsize()),
                              final_index,
                              lookup_result::kEOFUnit);
      }
      data_stream.rewind(_bpp);
      
      return lookup_result(bitfield_value(it->_codebook_value, get_bitsize()), table_index, unit);
   }

   // Does the operation of adding the new entry, signaling EOI, and signaling clear code
   std::optional<bitfield_value> lookup_phase_2(lookup_result const& last_result) {
      if (last_result._miss == lookup_result::kEOFUnit) {
         // Signal we need to write an EOI code
         return bitfield_value(eoi_code(), get_bitsize());
      }

      const uint16_t next_code = _codebook_size;

      if (next_code > kMaxCodebookEntry) {
         // Signal that we need to write a clear code
         bitfield_value ret(clear_code(), get_bitsize());
         _codebook_size = eoi_code() + 1;
         return ret;
      }

      _codebook_table[last_result._entry]._connections[last_result._miss] = _codebook_size;
      _codebook_table[_codebook_size].initialize(next_code, last_result._miss);
      _codebook_size++;

      return std::nullopt;
   }
};
}

// lzw_compress:
//    Compresses a stream of bits into a variable LZW format conforming to gif89a specification.
//    This includes clear & EOI codes.
template <std::size_t _bpp>
std::vector<uint8_t> lzw_compress(lzw::lsb_istream& in_stream) {
   auto codebook = lzw::lzw_codebook<_bpp>::alloc_codebook();

   std::vector<uint8_t> data_out;
   lzw::lsb_ostream out_stream(data_out, 0);

   // Append initial clear-code
   out_stream.append_some(codebook->clear_code_now());
   std::cout << "Wrote " << codebook->clear_code_now()._value << " (sz " << codebook->clear_code_now()._bit_size << ")" << std::endl;
   while (!in_stream.eof()) {
      lzw::lookup_result res = codebook->lookup_phase_1(in_stream);
      out_stream.append_some(res._output);
      std::cout << "Wrote " << res._output._value << " (sz " << static_cast<uint16_t>(res._output._bit_size) << ")" << std::endl;
      auto extra = codebook->lookup_phase_2(res);
      if (extra) {
         out_stream.append_some(*extra);
         std::cout << "Wrote " << extra->_value << " (sz " << static_cast<uint16_t>(extra->_bit_size) << ")" << std::endl;
      }
   }
   return std::move(data_out);
}
}

std::size_t build_sample_image(std::vector<uint8_t>& img_out) {
   lzw::lsb_ostream in_stream_build(img_out, 0);
   for (int i = 0; i < 3; i++) {
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
   }
   for (int i = 0; i < 2; i++) {
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
   }
   for (int i = 0; i < 2; i++) {
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
      in_stream_build.append_some(4, 3);
   }
   for (int i = 0; i < 3; i++) {
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(2, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
      in_stream_build.append_some(1, 3);
   }
   return in_stream_build.stream_size();
}

int main(int argc, char* argv[]) {
   std::vector<uint8_t> src;
   std::size_t num_bits = build_sample_image(src);

   lzw::lsb_istream in_stream(src, num_bits);
   std::vector<uint8_t> compress = std::move(lzw::lzw_compress<3>(in_stream));
   
   return 0;
}
