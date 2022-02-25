#include "gif_processor.hh"

#include <array>
#include <cassert>
#include <functional>
#include <iterator>
#include <optional>

#include "bitstream.hh"
#include "lzw.hh"
#include "quantize.hh"

namespace gifproc {
namespace {
constexpr std::string_view kGif87Magic = "GIF87a";
constexpr std::string_view kGif89Magic = "GIF89a";
constexpr std::string_view kNetscapeId = "NETSCAPE";
constexpr std::string_view kNetscapeAuth = "2.0";

struct color_table_info {
   constexpr color_table_info(std::vector<color_table_entry> const& table, uint8_t tp_idx)
         : _table(table), _tp_idx(tp_idx), _tp_present(true) {}
   constexpr color_table_info(std::vector<color_table_entry> const& table)
         : _table(table), _tp_idx(0), _tp_present(false) {}
   std::vector<color_table_entry> const& _table;
   uint8_t _tp_idx;
   bool _tp_present;
};

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

gif_parse_result gif::parse_image_data(std::size_t frame_number) {
   gif_frame_context& new_frame = _ctx->_frames.emplace_back();
   new_frame._frame_number = frame_number;

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
   std::size_t frame_num = 0;
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
            block_parse_result = parse_image_data(frame_num);
            frame_num++;
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

quant::dequant_params gif::dq_params_for_frame(gif_frame_context const& frame) const {
   static std::vector<color_table_entry> empty_table;
   if (empty_table.size() != 256) {
      empty_table.resize(256, {});
   }
   std::optional<uint8_t> transparent_idx = std::nullopt;
   std::optional<gif_disposal_method> disposal = std::nullopt;
   if (frame._extension) {
      transparent_idx = frame._extension->_transparent_enabled ?
                        std::make_optional<uint8_t>(frame._extension->_transparent_index) :
                        std::nullopt;
      disposal = frame._extension->_disposal_method;
   }

   if (frame._descriptor._lct_present) {
      return quant::dequant_params(frame._local_color_table, frame._min_code_size, frame._descriptor._interlaced,
                                   disposal, transparent_idx);
   } else if (_ctx->_lsd._gct_present) {
      return quant::dequant_params(_ctx->_global_color_table, frame._min_code_size, frame._descriptor._interlaced,
                                   disposal, transparent_idx);
   } else {
      return quant::dequant_params(empty_table, frame._min_code_size, frame._descriptor._interlaced, disposal,
                                   transparent_idx);
   }
}

quant::image gif::decode_image(gif_frame_context const& frame_ctx, quant::image const& last_frame) const {
   std::vector<uint8_t> compressed_data, decompressed_data;
   _raw_file.seekg(frame_ctx._image_data_start);
   for_each_subblock(_raw_file, [&compressed_data] (uint8_t* data, uint16_t len) {
         compressed_data.insert(compressed_data.end(), data, data + len);
         return gif_parse_result::kSuccess;
      });
   lzw::lzw_decode_result result = lzw::lzw_decompress(compressed_data, decompressed_data, frame_ctx._min_code_size);
   quant::image new_frame(_ctx->_lsd._canvas_width, _ctx->_lsd._canvas_height, frame_ctx._descriptor._image_left_pos,
                          frame_ctx._descriptor._image_top_pos, frame_ctx._descriptor._image_width,
                          frame_ctx._descriptor._image_height);

   quant::dequant_params params = dq_params_for_frame(frame_ctx);
   new_frame.prepare_frame(params, last_frame);
   new_frame.dequantize_from(params, decompressed_data, result._bits_written);
   return new_frame;
}

}
