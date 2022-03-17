#include "gif_processor.hh"

#include <array>
#include <cassert>
#include <cstring>
#include <functional>
#include <iterator>
#include <optional>

#include "bitstream.hh"
#include "dequantize.hh"
#include "lzw.hh"
#include "quantize.hh"

namespace gifproc {
namespace {
constexpr std::string_view kGif87Magic = "GIF87a";
static_assert(std::distance(kGif87Magic.begin(), kGif87Magic.end()) == sizeof(gif_header));

constexpr std::string_view kGif89Magic = "GIF89a";
static_assert(std::distance(kGif89Magic.begin(), kGif89Magic.end()) == sizeof(gif_header));

constexpr std::string_view kNetscapeId = "NETSCAPE";
static_assert(std::distance(kNetscapeId.begin(), kNetscapeId.end()) ==
              sizeof(application_extension::_application_identifier));

constexpr std::string_view kNetscapeAuth = "2.0";
static_assert(std::distance(kNetscapeAuth.begin(), kNetscapeAuth.end()) ==
              sizeof(application_extension::_authentication_code));

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
std::optional<T> stream_read(std::ifstream& in) {
   T val;
   in.read(reinterpret_cast<char*>(&val), sizeof(T));
   return in.gcount() == sizeof(T) ? std::make_optional(val) : std::nullopt;
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

gif::gif() : _dctx(nullptr), _ctx_debug(nullptr), _active_gce(std::nullopt) {}

gif::gif(gif&& rhs)
      : _dctx(std::move(rhs._dctx)),
        _ctx_debug(rhs._ctx_debug),
        _active_gce(std::nullopt),
        _raw_ifile(std::move(rhs._raw_ifile)) {}

gif_parse_result gif::open_read(std::string_view path) {
   _raw_ifile.open(path.data(), std::ios::binary);
   return parse_contents();
}

gif_parse_result gif::open_read(std::ifstream&& stream) {
   _raw_ifile = std::move(stream);
   return parse_contents();
}

gif_parse_result gif::parse_application_extension() {
   application_extension extension;
   if (!stream_read(_raw_ifile, &extension)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (!std::equal(kNetscapeId.begin(), kNetscapeId.end(), extension._application_identifier) ||
       !std::equal(kNetscapeAuth.begin(), kNetscapeAuth.end(), extension._authentication_code)) {
      return for_each_subblock(_raw_ifile, std::nullopt);
   }

   // Processing as subblocks to allow skipping unsupported application ext. types for netscape
   auto first_block_len = stream_read<uint8_t>(_raw_ifile);
   if (!first_block_len) {
      return gif_parse_result::kUnexpectedEof;
   }
   if (first_block_len != 3) {
      _raw_ifile.seekg(-1, std::ios::cur);
      return for_each_subblock(_raw_ifile, std::nullopt);
   }

   auto netscape_app_type = stream_read<uint8_t>(_raw_ifile);
   if (!netscape_app_type) {
      return gif_parse_result::kUnexpectedEof;
   }
   if (netscape_app_type != 0x01) {
      _raw_ifile.ignore(2);
      return gif_parse_result::kSuccess;
   }

   auto loop_count = stream_read<uint16_t>(_raw_ifile);
   if (!loop_count) {
      return gif_parse_result::kUnexpectedEof;
   }
   _dctx->_nse = netscape_extension { *loop_count };
   return gif_parse_result::kSuccess;
}

gif_parse_result gif::parse_extension() {
   auto extension_label = stream_read<uint8_t>(_raw_ifile);
   if (!extension_label) {
      return gif_parse_result::kUnexpectedEof;
   }

   switch (*extension_label) {
      case kGraphicsExtensionLabel: {
         auto block_size = stream_read<uint8_t>(_raw_ifile);
         if (!block_size) {
            return gif_parse_result::kUnexpectedEof;
         }
         if (block_size != kGraphicsExtensionSize) {
            return gif_parse_result::kInvalidBlockSize;
         }
         _active_gce = graphics_control_extension {};
         if (!stream_read(_raw_ifile, &_active_gce.value())) {
            return gif_parse_result::kUnexpectedEof;
         }
         auto block_term = stream_read<uint8_t>(_raw_ifile);
         if (!block_term) {
            return gif_parse_result::kUnexpectedEof;
         }
         if (block_term != 0) {
            return gif_parse_result::kInvalidBlockSize;
         }
         return gif_parse_result::kSuccess;
      }

      case kPlaintextExtensionLabel: {
         // Ignore this extension
         auto block_size = stream_read<uint8_t>(_raw_ifile);
         if (!block_size) {
            return gif_parse_result::kUnexpectedEof;
         }
         if (block_size != kPlaintextExtensionSize) {
            return gif_parse_result::kInvalidBlockSize;
         }
         _raw_ifile.ignore(sizeof(plaintext_extension));
         return for_each_subblock(_raw_ifile, std::nullopt);
      }

      case kApplicationExtensionLabel: {
         auto block_size = stream_read<uint8_t>(_raw_ifile);
         if (!block_size) {
            return gif_parse_result::kUnexpectedEof;
         }
         if (block_size != kApplicationExtensionSize) {
            return gif_parse_result::kInvalidBlockSize;
         }
         gif_parse_result extension_parse = parse_application_extension();
         if (extension_parse != gif_parse_result::kSuccess) {
            return extension_parse;
         }
         auto block_term = stream_read<uint8_t>(_raw_ifile);
         if (!block_term) {
            return gif_parse_result::kUnexpectedEof;
         }
         if (block_term != 0) {
            return gif_parse_result::kInvalidBlockSize;
         }
         return gif_parse_result::kSuccess;
      }

      case kCommentExtensionLabel: {
         std::string& comment_out = _dctx->_comments.emplace_back();
         return for_each_subblock(_raw_ifile, [&comment_out] (uint8_t* data, uint16_t len) {
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
   gif_frame_context& new_frame = _dctx->_frames.emplace_back();
   new_frame._frame_number = frame_number;

   if (_active_gce) {
      new_frame._extension = std::move(_active_gce);
   }

   if (!stream_read(_raw_ifile, &new_frame._descriptor)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (new_frame._descriptor._lct_present &&
       !read_color_table(_raw_ifile, new_frame._descriptor._lct_size, new_frame._local_color_table)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (!stream_read(_raw_ifile, &new_frame._min_code_size)) {
      return gif_parse_result::kUnexpectedEof;
   }

   new_frame._image_data_start = _raw_ifile.tellg();
   // Skip image data to be loaded later
   return for_each_subblock(_raw_ifile, std::nullopt);
}

gif_parse_result gif::parse_contents() {
   if (!_raw_ifile.is_open()) {
      return gif_parse_result::kFileNotFound;
   }

   _dctx = std::make_unique<deserialized_gif_context>();
   _ctx_debug = _dctx.get();
   auto version_opt = parse_gif_version(_raw_ifile);
   if (!version_opt) {
      return gif_parse_result::kInvalidHeader;
   }
   _dctx->_version = *version_opt;

   if (!stream_read(_raw_ifile, &_dctx->_lsd)) {
      return gif_parse_result::kUnexpectedEof;
   }

   if (_dctx->_lsd._gct_present &&
       !read_color_table(_raw_ifile, _dctx->_lsd._gct_size, _dctx->_global_color_table)) {
      return gif_parse_result::kUnexpectedEof;
   }

   // With the heading information out of the way, now we need to process a series of frames & extension blocks
   bool trailer_found = false;
   std::size_t frame_num = 0;
   while (!trailer_found && !_raw_ifile.eof()) {
      auto next_block = stream_read<uint8_t>(_raw_ifile);
      if (!next_block) {
         return gif_parse_result::kUnexpectedEof;
      }

      bool destroy_active_gce = _active_gce.has_value();

      gif_parse_result block_parse_result = gif_parse_result::kSuccess;
      switch (*next_block) {
         case kExtensionIntroducer:
            if (_dctx->_version != gif_version::kGif89a) {
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

quant::gif_frame gif::decode_image(gif_frame_context const& frame_ctx, quant::gif_frame const& last_frame) const {
   std::vector<uint8_t> compressed_data, decompressed_data;
   _raw_ifile.seekg(frame_ctx._image_data_start);
   for_each_subblock(_raw_ifile, [&compressed_data] (uint8_t* data, uint16_t len) {
         compressed_data.insert(compressed_data.end(), data, data + len);
         return gif_parse_result::kSuccess;
      });
   lzw::lzw_decode_result result = lzw::lzw_decompress(compressed_data, decompressed_data, frame_ctx._min_code_size);

   std::optional<uint8_t> transparent_index = std::nullopt;
   std::optional<gif_disposal_method> disposal_method = std::nullopt;
   if (frame_ctx._extension) {
      transparent_index = frame_ctx._extension->_transparent_enabled ?
                          std::make_optional(frame_ctx._extension->_transparent_index) :
                          std::nullopt;
      disposal_method = frame_ctx._extension->_disposal_method;
   }
   quant::qimg quant_frame(std::move(decompressed_data), frame_ctx._min_code_size, result._bits_written,
                           frame_ctx._descriptor._image_left_pos, frame_ctx._descriptor._image_top_pos,
                           frame_ctx._descriptor._image_width, frame_ctx._descriptor._image_height, transparent_index);
   quant::dequant_params params(frame_ctx._descriptor._interlaced, disposal_method);

   if (frame_ctx._descriptor._lct_present) {
      quant_frame._palette = frame_ctx._local_color_table;
   } else if (_dctx->_lsd._gct_present) {
      quant_frame._palette = _dctx->_global_color_table;
   } else {
      assert(false);
   }

   quant::gif_frame new_frame(_dctx->_lsd._canvas_width, _dctx->_lsd._canvas_height, quant_frame._x, quant_frame._y,
                              quant_frame._w, quant_frame._h);

   prepare_frame(new_frame, params, last_frame);
   dequantize_from(new_frame, params, quant_frame);
   return new_frame;
}

void gif::open_write(std::string_view path) {
   _sctx = std::make_unique<serialized_gif_context>();
   _sctx->_max_w = 0;
   _sctx->_max_h = 0;
   _sctx->_required_version = gif_version::kGif87a;
   _raw_ofile.open(path.data(), std::ios::binary);
   // Buffer space for our header and LSD, filled on final write
   std::fill_n(std::ostream_iterator<char>(_raw_ofile), sizeof(gif_header) + sizeof(logical_screen_descriptor), 0);
   std::fill_n(std::ostream_iterator<char>(_raw_ofile), 256 * sizeof(color_table_entry), 0);
   std::fill_n(std::ostream_iterator<char>(_raw_ofile), 3 + sizeof(application_extension) + 5, 0);
}

void gif::add_frame(piximg const& frame, std::optional<uint16_t> delay) {
   if (_sctx == nullptr || !_raw_ofile.is_open()) {
      return;
   }
   quant::qimg quant_frame;
   quant::quantize(frame, quant_frame);
}

void gif::add_frame(quant::qimg const& quant_frame, std::optional<uint16_t> delay) {
   _sctx->_max_w = quant_frame._w;
   _sctx->_max_h = quant_frame._h;

   if (quant_frame._t_index || delay) {
      _sctx->_required_version = gif_version::kGif89a;

      graphics_control_extension gce;
      gce._transparent_enabled = quant_frame._t_index.has_value();
      gce._user_input = false;
      gce._disposal_method = gif_disposal_method::kNone;
      gce._reserved_0 = 0;
      gce._delay_time = delay ? *delay : 0;
      gce._transparent_index = *quant_frame._t_index;
      _raw_ofile.put(kExtensionIntroducer);
      _raw_ofile.put(kGraphicsExtensionLabel);
      _raw_ofile.put(sizeof(graphics_control_extension));
      _raw_ofile.write(reinterpret_cast<const char*>(&gce), sizeof(graphics_control_extension));
      _raw_ofile.put(0);
   }

   {
      image_descriptor desc;
      desc._image_left_pos = quant_frame._x;
      desc._image_top_pos = quant_frame._y;
      desc._image_width = quant_frame._w;
      desc._image_height = quant_frame._h;
      desc._lct_size = 0;
      desc._reserved_1 = 0;
      desc._sorted = true;
      desc._interlaced = false;
      desc._lct_present = !quant_frame._palette.empty();
      _raw_ofile.put(kImageSeparator);
      _raw_ofile.write(reinterpret_cast<const char*>(&desc), sizeof(image_descriptor));

      if (desc._lct_present) {
         const int pad_to = 256 * sizeof(color_table_entry);
         for (color_table_entry const& cte : quant_frame._palette) {
            _raw_ofile.write(reinterpret_cast<const char*>(&cte), sizeof(color_table_entry));
         }
         
         std::fill_n(std::ostream_iterator<char>(_raw_ofile),
                     pad_to - (sizeof(color_table_entry) * quant_frame._palette.size()),
                     0);
      }
   }

   {
      constexpr auto subblock_write = [](std::vector<uint8_t> const& data, std::ofstream& file) {
         constexpr std::size_t kMaxBlockSz = 255;
         for (std::size_t i = 0; i < data.size(); i += kMaxBlockSz) {
            std::size_t block_size = (i + kMaxBlockSz) > data.size() ? data.size() - i : kMaxBlockSz;
            file.put(static_cast<char>(block_size));
            file.write(reinterpret_cast<const char*>(data.data() + i), block_size);
         }
         file.put(0);
      };

      std::vector<uint8_t> compressed_frame;
      lzw::lzw_compress(quant_frame._index, quant_frame._nbits, quant_frame._bpp, compressed_frame);
      _raw_ofile.put(quant_frame._bpp);
      subblock_write(compressed_frame, _raw_ofile);
   }
}

void gif::finish_write() {
   if (_sctx == nullptr || !_raw_ofile.is_open()) {
      return;
   }

   _raw_ofile.put(kGifTrailer);
   _raw_ofile.seekp(0, std::ios::beg);

   gif_header header;
   if (_sctx->_required_version == gif_version::kGif87a) {
      std::copy(kGif87Magic.begin(), kGif87Magic.end(), header._version);
   } else {
      std::copy(kGif89Magic.begin(), kGif89Magic.end(), header._version);
   }
   _raw_ofile.write(reinterpret_cast<const char*>(&header), sizeof(gif_header));

   logical_screen_descriptor lsd;
   lsd._canvas_width = _sctx->_max_w;
   lsd._canvas_height = _sctx->_max_h;
   lsd._gct_size = 7;
   lsd._sort_flag = false;
   lsd._color_resolution = 0;
   lsd._gct_present = true;
   lsd._bg_color_index = 0;
   lsd._pixel_aspect_ratio = 0;

   _raw_ofile.write(reinterpret_cast<const char*>(&lsd), sizeof(logical_screen_descriptor));

   _raw_ofile.seekp(sizeof(gif_header) + sizeof(logical_screen_descriptor) + 256 * sizeof(color_table_entry));
   application_extension ext;
   std::copy(kNetscapeId.begin(), kNetscapeId.end(), ext._application_identifier);
   std::copy(kNetscapeAuth.begin(), kNetscapeAuth.end(), ext._authentication_code);
   _raw_ofile.put(kExtensionIntroducer);
   _raw_ofile.put(kApplicationExtensionLabel);
   _raw_ofile.put(kApplicationExtensionSize);
   _raw_ofile.write(reinterpret_cast<const char*>(&ext), sizeof(application_extension));
   _raw_ofile.put(3);
   _raw_ofile.put(1);
   _raw_ofile.put(0);
   _raw_ofile.put(0);
   _raw_ofile.put(0);
}

void gif::finish_write(std::vector<color_table_entry> const& gct) {
   finish_write();
   _raw_ofile.seekp(sizeof(gif_header) + sizeof(logical_screen_descriptor));
   for (color_table_entry const& e : gct) {
      _raw_ofile.write(reinterpret_cast<const char*>(&e), sizeof(color_table_entry));
   }
}

}
