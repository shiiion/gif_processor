#include "gif_processor.hh"

#include <array>
#include <cassert>
#include <functional>
#include <iterator>
#include <optional>

#include "bitstream.hh"
#include "lzw.hh"

namespace gifproc {
namespace {
constexpr std::string_view kGif87Magic = "GIF87a";
constexpr std::string_view kGif89Magic = "GIF89a";
constexpr std::string_view kNetscapeId = "NETSCAPE";
constexpr std::string_view kNetscapeAuth = "2.0";

std::optional<gif_version> parse_gif_version(std::ifstream& in) {
   gif_header header;
   in.read(header._version, sizeof(gif_header::_version));
   if (std::equal(kGif87Magic.begin(), kGif87Magic.end(), header._version)) {
      return gif_version::kGif87a;
   } else if (std::equal(kGif89Magic.begin(), kGif89Magic.end(), header._version)) {
      return gif_version::kGif89a;
   }
   return std::nullopt;
}

template <typename T>
bool stream_read(std::ifstream& in, T* output) {
   in.read(reinterpret_cast<char*>(output), sizeof(T));
   return in.gcount() == sizeof(T);
}

template <typename _It>
bool stream_read_container(std::ifstream& in, _It begin, _It end) {
   using T = typename std::iterator_traits<_It>::value_type;
   for (_It i = begin; i != end; i++) {
      in.read(reinterpret_cast<char*>(&(*i)), sizeof(T));
      if (in.gcount() != sizeof(T)) {
         return false;
      }
   }
   return true;
}

bool read_color_table(std::ifstream& in, uint8_t table_bits, std::vector<color_table_entry>& table_out) {
   // Invalid files will not cause this, so assert to be safe
   assert(table_bits < 8);
   table_out.resize(1 << (table_bits + 1));
   return stream_read_container(in, table_out.begin(), table_out.end());
}

gif_parse_result for_each_subblock(std::ifstream& in,
                                   std::optional<std::function<gif_parse_result(uint8_t*, uint16_t)>>&& func) {
   uint16_t subblock_len;
   std::array<uint8_t, 255> subblock;
   for (;;) {
      subblock_len = static_cast<uint16_t>(in.get());
      if (in.gcount() != 1) {
         return gif_parse_result::kUnexpectedEof;
      }
      if (subblock_len == 0) {
         break;
      }

      if (func) {
         if (!stream_read_container(in, subblock.begin(), subblock.begin() + subblock_len)) {
            return gif_parse_result::kUnexpectedEof;
         }
         gif_parse_result cb_result = (*func)(subblock.data(), subblock_len);
         if (cb_result != gif_parse_result::kSuccess) {
            return cb_result;
         }
      } else {
         in.ignore(subblock_len);
         if (in.gcount() != subblock_len) {
            return gif_parse_result::kUnexpectedEof;
         }
      }
   }
   return gif_parse_result::kSuccess;
}

template <std::size_t _Bits>
void dequantize_image(std::vector<uint8_t> const& q_in,
                      std::size_t q_bits,
                      std::vector<uint8_t>& dq_out,
                      std::vector<color_table_entry> const& color_table) {
   dq_out.clear();
   util::cbw_istream<_Bits> in(q_in, q_bits);
   while (!in.eof()) {
      uint8_t val;
      in >> val;
      dq_out.push_back(color_table[val]._red);
      dq_out.push_back(color_table[val]._green);
      dq_out.push_back(color_table[val]._blue);
      dq_out.push_back(255);
   }
}

void dequantize_image(std::vector<uint8_t> const& q_in,
                      std::size_t q_bits,
                      std::vector<uint8_t>& dq_out,
                      std::vector<color_table_entry> const& color_table,
                      uint8_t bits) {
   switch (bits) {
      case 2:
         dequantize_image<2>(q_in, q_bits, dq_out, color_table);
         break;
      case 3:
         dequantize_image<3>(q_in, q_bits, dq_out, color_table);
         break;
      case 4:
         dequantize_image<4>(q_in, q_bits, dq_out, color_table);
         break;
      case 5:
         dequantize_image<5>(q_in, q_bits, dq_out, color_table);
         break;
      case 6:
         dequantize_image<6>(q_in, q_bits, dq_out, color_table);
         break;
      case 7:
         dequantize_image<7>(q_in, q_bits, dq_out, color_table);
         break;
      case 8:
         dequantize_image<8>(q_in, q_bits, dq_out, color_table);
         break;
      default:
         assert(false);
         break;
   }
}
}

gif::gif() : _ctx(nullptr), _ctx_debug(nullptr), _active_gce(std::nullopt) {}

gif::gif(gif&& rhs) : _ctx(std::move(rhs._ctx)), _ctx_debug(rhs._ctx_debug), _active_gce(std::nullopt), _raw_file(std::move(rhs._raw_file)) {}

gif_parse_result gif::open(std::string_view path) {
   _raw_file.open(path.data(), std::ios::binary);
   return parse_contents();
}

gif_parse_result gif::open(std::ifstream&& stream) {
   _raw_file = std::move(stream);
   return parse_contents();
}

gif_parse_result gif::parse_application_extension() {
   application_extension extension;
   if (!stream_read(_raw_file, &extension)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (!std::equal(kNetscapeId.begin(), kNetscapeId.end(), extension._application_identifier) ||
       !std::equal(kNetscapeAuth.begin(), kNetscapeAuth.end(), extension._authentication_code)) {
      return for_each_subblock(_raw_file, std::nullopt);
   }

   return for_each_subblock(_raw_file, [this] (uint8_t* data, uint16_t len) {
         if (len != 3 || data[0] != 0x01) {
            return gif_parse_result::kInvalidApplicationData;
         }
         uint16_t loop_count = *reinterpret_cast<uint16_t*>(data + 1);
         _ctx->_nse = netscape_extension { loop_count };
         return gif_parse_result::kSuccess;
      });
}

gif_parse_result gif::parse_extension() {
   uint8_t extension_label = static_cast<uint8_t>(_raw_file.get());
   if (_raw_file.gcount() != 1) {
      return gif_parse_result::kUnexpectedEof;
   }

   switch (extension_label) {
      case kGraphicsExtensionLabel: {
         _active_gce = graphics_control_extension {};
         return for_each_subblock(_raw_file, [this] (uint8_t* data, uint16_t len) {
               if (len != 4) {
                  return gif_parse_result::kInvalidGCEData;
               }
               std::copy(data, data + len, reinterpret_cast<uint8_t*>(&_active_gce.value()));
               return gif_parse_result::kSuccess;
            });
      }

      case kPlaintextExtensionLabel: {
         // Ignore this extension
         _raw_file.ignore(sizeof(plaintext_extension));
         return for_each_subblock(_raw_file, std::nullopt);
      }

      case kApplicationExtensionLabel: {
         return parse_application_extension();
      }

      case kCommentExtensionLabel: {
         std::string& comment_out = _ctx->_comments.emplace_back();
         return for_each_subblock(_raw_file, [&comment_out] (uint8_t* data, uint16_t len) {
               comment_out.append(reinterpret_cast<char*>(data), len);
               return gif_parse_result::kSuccess;
            });
      }

      default: {
         return gif_parse_result::kInvalidExtensionLabel;
      }
   }
}

gif_parse_result gif::parse_image_data() {
   gif_frame_context& new_frame = _ctx->_frames.emplace_back();

   if (_active_gce) {
      new_frame._extension = std::move(_active_gce);
   }

   if (!stream_read(_raw_file, &new_frame._descriptor)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (new_frame._descriptor._lct_present &&
       !read_color_table(_raw_file, new_frame._descriptor._lct_size, new_frame._local_color_table)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (!stream_read(_raw_file, &new_frame._min_code_size)) {
      return gif_parse_result::kUnexpectedEof;
   }

   new_frame._image_data_start = _raw_file.tellg();
   // Skip image data to be loaded later
   return for_each_subblock(_raw_file, std::nullopt);
}

gif_parse_result gif::parse_contents() {
   if (!_raw_file.is_open()) {
      return gif_parse_result::kFileNotFound;
   }

   _ctx = std::make_unique<deserialized_gif_context>();
   _ctx_debug = _ctx.get();
   auto version_opt = parse_gif_version(_raw_file);
   if (!version_opt) {
      return gif_parse_result::kInvalidHeader;
   }
   _ctx->_version = *version_opt;

   if (!stream_read(_raw_file, &_ctx->_lsd)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (_ctx->_lsd._gct_present &&
       !read_color_table(_raw_file, _ctx->_lsd._gct_size, _ctx->_global_color_table)) {
      return gif_parse_result::kUnexpectedEof;
   }

   // With the heading information out of the way, now we need to process a series of frames & extension blocks
   bool trailer_found = false;
   while (!trailer_found && !_raw_file.eof()) {
      uint8_t next_block = static_cast<uint8_t>(_raw_file.get());
      if (_raw_file.gcount() != 1) {
         return gif_parse_result::kUnexpectedEof;
      }

      bool destroy_active_gce = _active_gce.has_value();

      gif_parse_result block_parse_result = gif_parse_result::kSuccess;
      switch (next_block) {
         case kExtensionIntroducer:
            if (_ctx->_version != gif_version::kGif89a) {
               return gif_parse_result::kNotSupported;
            }
            block_parse_result = parse_extension();
            break;
         case kImageSeparator:
            block_parse_result = parse_image_data();
            break;
         case kGifTrailer:
            trailer_found = true;
            break;
      }

      if (block_parse_result != gif_parse_result::kSuccess) {
         return block_parse_result;
      }

      if (destroy_active_gce) {
         _active_gce = std::nullopt;
      }
   }
   return trailer_found ? gif_parse_result::kSuccess : gif_parse_result::kUnexpectedEof;
}

void gif::decode_image(gif_frame_context const& frame_ctx, std::vector<uint8_t>& image_out) const {
   std::vector<uint8_t> compressed_data, decompressed_data;
   _raw_file.seekg(frame_ctx._image_data_start);
   for_each_subblock(_raw_file, [&compressed_data] (uint8_t* data, uint16_t len) {
         compressed_data.insert(compressed_data.end(), data, data + len);
         return gif_parse_result::kSuccess;
      });
   lzw::lzw_decode_result result = lzw::lzw_decompress(compressed_data, decompressed_data, frame_ctx._min_code_size);
   if (frame_ctx._descriptor._lct_present) {
      dequantize_image(decompressed_data,
                       result._bits_written,
                       image_out,
                       frame_ctx._local_color_table,
                       frame_ctx._descriptor._lct_size + 1);
   } else if (_ctx->_lsd._gct_present) {
      dequantize_image(decompressed_data,
                       result._bits_written,
                       image_out,
                       _ctx->_global_color_table,
                       _ctx->_lsd._gct_size + 1);
   } else {
      printf("No color table available fixme\n");
   }

}

}
