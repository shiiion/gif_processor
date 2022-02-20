#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <optional>

#include "lzw.hh"

namespace gifproc::lzw {
namespace {

using gifproc::util::bitfld;
using codebook_reference = uint16_t;
using lzw_bitfld = bitfld<uint16_t>;


struct lookup_result {
   constexpr static uint16_t kEOFUnit = 0xffff;
   lzw_bitfld _output;
   codebook_reference _entry;
   uint16_t _miss;

   constexpr lookup_result(bitfld<uint16_t> output, codebook_reference entry, uint16_t miss)
         : _output(output), _entry(entry), _miss(miss) {}
};

template <std::size_t _Bits>
class _codebook_base {
protected:
   constexpr static uint16_t kMaxCodebookEntries = static_cast<uint16_t>(1 << 12);

   uint16_t _codebook_size;

   constexpr uint16_t clear_code() const {
      return uint16_t{1} << _Bits;
   }

   constexpr uint16_t eoi_code() const {
      return clear_code() + 1;
   }

   constexpr uint8_t get_bitsize() const {
      // The number of bits for variable lzw is dependant on the highest (currently) generatable value:
      //    floor(log2(_codebook_size))
      // This will always be at least one greater than the _Bits
      return 32 - __builtin_clz(static_cast<unsigned int>(_codebook_size - 1));
   }

   _codebook_base(std::size_t initial_size) : _codebook_size(initial_size) {}

public:
   constexpr lzw_bitfld clear_code_now() const {
      return util::create_nbits(clear_code(), get_bitsize());
   }
};

template <std::size_t _Bits>
class compress_codebook : public _codebook_base<_Bits> {
private:
   using base_type = _codebook_base<_Bits>;

   // codebook_entry defines a single node within the codebook trie
   struct codebook_entry {
      constexpr static uint16_t kInvalidConnection = 0xffff;

      std::array<uint16_t, 1 << _Bits> _connections;
      uint16_t _codebook_value;

      void initialize(uint16_t value = 0) {
         _connections.fill(kInvalidConnection);
         _codebook_value = value;
      }
   };

   codebook_entry _codebook_head;
   std::array<codebook_entry, base_type::kMaxCodebookEntries> _codebook_table;
   uint16_t _codebook_size;

   compress_codebook() : _codebook_base<_Bits>(base_type::eoi_code() + 1) {
      _codebook_head.initialize();
      for (uint16_t i = 0; i < _codebook_size; i++) {
         _codebook_table[i].initialize(i);
         _codebook_head._connections[i] = i;
      }
   }

public:
   static std::unique_ptr<compress_codebook<_Bits>> alloc_codebook() {
      return std::unique_ptr<compress_codebook<_Bits>>(new compress_codebook<_Bits>());
   }

   // Does the operation of looking up an entry, breaking at the first miss
   // Moves the bitstream position to the location of the miss
   lookup_result lookup_phase_1(util::cbw_istream<_Bits>& data_stream) const {
      uint16_t unit = data_stream.read_extract();
      uint16_t table_index = codebook_entry::kInvalidConnection;

      codebook_entry const* it = &_codebook_head;
      while (it->_connections[unit] != codebook_entry::kInvalidConnection &&
             !data_stream.eof()) {
         it = &_codebook_table[table_index = it->_connections[unit]];
         unit = data_stream.read_extract();
      }
      // If we reach EOF and the last unit makes up a fully mapped sequence
      if (it->_connections[unit] != codebook_entry::kInvalidConnection) {
         const uint16_t final_index = it->_connections[unit];
         return lookup_result(util::create_nbits<uint16_t>(
                                 _codebook_table[final_index]._codebook_value,
                                 base_type::get_bitsize()),
                              final_index,
                              lookup_result::kEOFUnit);
      }
      data_stream.rewind(1);
      
      return lookup_result(util::create_nbits(it->_codebook_value, base_type::get_bitsize()), table_index, unit);
   }

   // Does the operation of adding the new entry, signaling EOI, and signaling clear code
   std::optional<lzw_bitfld> lookup_phase_2(lookup_result const& last_result) {
      if (last_result._miss == lookup_result::kEOFUnit) {
         // Signal we need to write an EOI code
         return util::create_nbits(base_type::eoi_code(), base_type::get_bitsize());
      }

      const uint16_t next_code = _codebook_size;

      if (next_code == base_type::kMaxCodebookEntries) {
         // Signal that we need to write a clear code
         lzw_bitfld ret = util::create_nbits(base_type::clear_code(), base_type::get_bitsize());
         _codebook_size = base_type::eoi_code() + 1;
         return ret;
      }

      _codebook_table[last_result._entry]._connections[last_result._miss] = _codebook_size;
      _codebook_table[_codebook_size].initialize(next_code);
      _codebook_size++;

      return std::nullopt;
   }
};

// lzw_compress_generic:
//    Compresses a stream of bits into a variable LZW format conforming to gif89a specification.
//    This includes clear & EOI codes.
template <std::size_t _Bits>
void lzw_compress_generic(util::cbw_istream<_Bits>& in, util::vbw_ostream& out) {
   auto codebook = compress_codebook<_Bits>::alloc_codebook();

   // Append initial clear-code
   out.write(codebook->clear_code_now());
   while (!in.eof()) {
      // lookup_phase_1 will scan through the input stream until it finds a sequence not in its codebook.
      // The value of the last matched sequence's key will be returned in lookup_result::_output.
      // The codebook entry which the miss occurred at will be returned in lookup_result::_entry.
      // The stream unit which the miss occurred on will be returned in lookup_result::_miss.
      lookup_result res = codebook->lookup_phase_1(in);
      out.write(res._output);

      // lookup_phase_2 will write the new key to the dictionary, and possibly give us back an extra few bits to
      // write to our stream. extra will be one of either a clear code, signifying that phase 2 has cleared the
      // dictionary, or an EOI code, signifying that we've reached the end of our data stream.
      auto extra = codebook->lookup_phase_2(res);
      if (extra) {
         out.write(*extra);
      }
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// LZW DECOMPRESSION ///////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <std::size_t _Bits>
class decompress_codebook : _codebook_base<_Bits> {
private:
   using base_type = _codebook_base<_Bits>;

   // codebook_entry defines a single node within the codebook trie
   // These entries are organized in a reverse-tree fashion (nodes point to their parents), and we re-build the
   // compression trie
   struct codebook_entry {
      constexpr static uint16_t kInvalidConnection = 0xffff;

      uint16_t _parent;

      // In order to decompress a code, we need to trace-back where a leaf/inner-node sourced from
      // To reverse this process, we save the current node to its parent (here) so that we can re-trace our steps
      // The reason for storing it here is just for the convenience as well as speed, as a "stack" defaults to
      // dynamically allocated structures
      uint16_t _tmp_next_entry;

      uint8_t _decoded_index;

      // To speed up lookups, we just store the initial index directly in a node
      uint8_t _base_index;

      void initialize(uint16_t parent, uint8_t decoded_index, uint8_t base_index) {
         _parent = parent;
         _tmp_next_entry = kInvalidConnection;
         _decoded_index = decoded_index;
         _base_index = base_index;
      }
   };

   std::array<codebook_entry, base_type::kMaxCodebookEntries> _codebook_table;
   uint16_t _prev_code;

   decompress_codebook()
         : _codebook_base<_Bits>(base_type::eoi_code() + 1),
           _prev_code(codebook_entry::kInvalidConnection) {
      for (uint16_t i = 0; i < base_type::_codebook_size; i++) {
         _codebook_table[i].initialize(codebook_entry::kInvalidConnection,
                                       static_cast<uint8_t>(i),
                                       static_cast<uint8_t>(i));
      }
   }

   // Decompress a code by back-tracing the head of its sequence and caching the path, then running forward from the
   // head node
   void decompress_impl(uint16_t code, util::cbw_ostream<_Bits>& out) {
      uint16_t cur_node = code;
      _codebook_table[cur_node]._tmp_next_entry = codebook_entry::kInvalidConnection;

      // back-trace and cache
      while (_codebook_table[cur_node]._parent != codebook_entry::kInvalidConnection) {
         uint16_t child_node = cur_node;
         cur_node = _codebook_table[cur_node]._parent;
         _codebook_table[cur_node]._tmp_next_entry = child_node;
      }

      // run forward and output
      while (cur_node != codebook_entry::kInvalidConnection) {
         out << _codebook_table[cur_node]._decoded_index;
         cur_node = _codebook_table[cur_node]._tmp_next_entry;
      }
   }

public:
   static std::unique_ptr<decompress_codebook<_Bits>> alloc_codebook() {
      return std::unique_ptr<decompress_codebook<_Bits>>(new decompress_codebook<_Bits>());
   }

   decompress_status check_initial_clear_code(util::vbw_istream& in, util::cbw_ostream<_Bits>& out) {
      if (in.eof()) {
         return decompress_status::kUnexpectedEof;
      }

      const uint16_t start_code = static_cast<uint16_t>(in.read_extract(base_type::get_bitsize()));
      if (start_code != base_type::clear_code()) {
         return decompress_status::kMissingInitialClearCode;
      }

      if (in.eof()) {
         return decompress_status::kUnexpectedEof;
      }
   }

   decompress_status decompress_single_code(util::vbw_istream& in, util::cbw_ostream<_Bits>& out) {
      // Based on how this is called, this should not be hit, but it gives me peace of mind
      if (in.eof()) {
         return decompress_status::kUnexpectedEof;
      }

      const uint16_t cur_code = static_cast<uint16_t>(in.read_extract(base_type::get_bitsize()));
      // Handle EOI and clear codes
      if (cur_code == base_type::eoi_code()) {
         in.seek_end();
         return decompress_status::kSuccess;
      } else if (cur_code == base_type::clear_code()) {
         base_type::_codebook_size = base_type::eoi_code() + 1;
         _prev_code = codebook_entry::kInvalidConnection;
         return decompress_status::kSuccess;
      }

      // This is where EOF should be checked. If EOI was not discovered above, then we've got a data stream missing an
      // end code.
      if (in.eof()) {
         return decompress_status::kUnexpectedEof;
      }

      // If a clear code was not seen above but the maximum number of codebook entries has been reached, then something
      // went bunk
      if (base_type::_codebook_size == base_type::kMaxCodebookEntries) {
         return decompress_status::kDictionaryOverflow;
      }


      if (cur_code == base_type::_codebook_size) {
         // Very special case, if a clear code was just sent then we can't infer any previous info, thus an invalid
         // _prev_code value means the encoded data is bad
         if (_prev_code >= base_type::_codebook_size) {
            return decompress_status::kInvalidCompressCode;
         }

         // Write out the decompressed indices of our previous code, plus the initial index of it
         decompress_impl(_prev_code, out);
         out << _codebook_table[_prev_code]._base_index;

         // New dictionary entry consists of the start code of the previous sequence
         _codebook_table[base_type::_codebook_size].initialize(_prev_code,
                                                               _codebook_table[_prev_code]._base_index,
                                                               _codebook_table[_prev_code]._base_index);
      } else if (cur_code < base_type::_codebook_size) {
         // We may need to handle a just-cleared codebook, signified by _prev_code == codebook_entry::kInvalidConnection
         if (_prev_code == codebook_entry::kInvalidConnection) {
            out << _codebook_table[cur_code]._base_index;
         } else {
            // First, write out the decompressed code to our output stream
            decompress_impl(cur_code, out);
            // Then handle adding a new code to our codebook
            _codebook_table[base_type::_codebook_size].initialize(_prev_code,
                                                                  _codebook_table[cur_code]._base_index,
                                                                  _codebook_table[cur_code]._base_index);
         }
      } else {
         // Invalid code, we can't infer what the encoder was doing
         return decompress_status::kInvalidCompressCode;
      }

      _prev_code = cur_code;
      base_type::_codebook_size++;

      return decompress_status::kSuccess;
   }
};

template <std::size_t _Bits>
decompress_status lzw_decompress_generic(util::vbw_istream& in, util::cbw_ostream<_Bits>& out) {
   auto codebook = decompress_codebook<_Bits>::alloc_codebook();

   decompress_status status = codebook->check_initial_clear_code(in, out);
   if (status != decompress_status::kSuccess) {
      return status;
   }

   while (!in.eof()) {
      codebook->decompress_single_code(in, out);
      if (status != decompress_status::kSuccess) {
         return status;
      }
   }

   return decompress_status::kSuccess;
}
}


void lzw_compress_1bpp(util::cbw_istream<1>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}

void lzw_compress_2bpp(util::cbw_istream<2>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}

void lzw_compress_3bpp(util::cbw_istream<3>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}

void lzw_compress_4bpp(util::cbw_istream<4>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}

void lzw_compress_5bpp(util::cbw_istream<5>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}

void lzw_compress_6bpp(util::cbw_istream<6>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}

void lzw_compress_7bpp(util::cbw_istream<7>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}

void lzw_compress_8bpp(util::cbw_istream<8>& in, util::vbw_ostream& out) {
   lzw_compress_generic(in, out);
}










decompress_status lzw_decompress_1bpp(util::vbw_istream& in, util::cbw_ostream<1>& out) {
   return lzw_decompress_generic(in, out);
}

decompress_status lzw_decompress_2bpp(util::vbw_istream& in, util::cbw_ostream<2>& out) {
   return lzw_decompress_generic(in, out);
}

decompress_status lzw_decompress_3bpp(util::vbw_istream& in, util::cbw_ostream<3>& out) {
   return lzw_decompress_generic(in, out);
}

decompress_status lzw_decompress_4bpp(util::vbw_istream& in, util::cbw_ostream<4>& out) {
   return lzw_decompress_generic(in, out);
}

decompress_status lzw_decompress_5bpp(util::vbw_istream& in, util::cbw_ostream<5>& out) {
   return lzw_decompress_generic(in, out);
}

decompress_status lzw_decompress_6bpp(util::vbw_istream& in, util::cbw_ostream<6>& out) {
   return lzw_decompress_generic(in, out);
}

decompress_status lzw_decompress_7bpp(util::vbw_istream& in, util::cbw_ostream<7>& out) {
   return lzw_decompress_generic(in, out);
}

decompress_status lzw_decompress_8bpp(util::vbw_istream& in, util::cbw_ostream<8>& out) {
   return lzw_decompress_generic(in, out);
}

}

std::size_t build_sample_image(std::vector<uint8_t>& img_out) {
   gifproc::util::cbw_ostream<3> img_build(img_out, 0);
   for (int i = 0; i < 3; i++) {
      img_build.write(1);
      img_build.write(1);
      img_build.write(1);
      img_build.write(1);
      img_build.write(1);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
   }
   for (int i = 0; i < 2; i++) {
      img_build.write(4);
      img_build.write(4);
      img_build.write(4);
      img_build.write(4);
      img_build.write(4);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
   }
   for (int i = 0; i < 2; i++) {
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(4);
      img_build.write(4);
      img_build.write(4);
      img_build.write(4);
      img_build.write(4);
   }
   for (int i = 0; i < 3; i++) {
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(2);
      img_build.write(1);
      img_build.write(1);
      img_build.write(1);
      img_build.write(1);
      img_build.write(1);
   }
   return img_build.size();
}

int main(int argc, char** argv) {
   std::vector<uint8_t> vec;
   std::vector<uint8_t> ovec;

   auto sz = build_sample_image(vec);
   gifproc::util::cbw_istream<2> in(vec, sz);
   gifproc::util::vbw_ostream out(ovec);
   gifproc::lzw::lzw_compress(in, out);

}
