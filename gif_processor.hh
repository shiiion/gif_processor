#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gifproc {

#pragma pack(push, 1)
struct color_table_entry {
   uint8_t _red;
   uint8_t _green;
   uint8_t _blue;
};

struct logical_screen_descriptor {
   uint16_t _canvas_width;
   uint16_t _canvas_height;

   bool _gct_present : 1;
   uint8_t _color_resolution : 3;
   bool _sort_flag : 1;
   uint8_t _gct_size : 3;

   uint8_t _bg_color_index;
   uint8_t _pixel_aspect_ratio = 0;
};

enum class gif_version {
   kGif87a,
   kGif89a,
};
struct gif_header {
   char _version[6];
};
constexpr uint8_t kGifTrailer = 0x3b;
constexpr uint8_t kExtensionIntroducer = 0x21;
constexpr uint8_t kImageSeparator = 0x2c;

constexpr uint8_t kGraphicsExtensionLabel = 0xf9;
constexpr uint8_t kPlaintextExtensionLabel = 0x01;
constexpr uint8_t kApplicationExtensionLabel = 0xff;
constexpr uint8_t kCommentExtensionLabel = 0xfe;
constexpr uint8_t kBlockTerminator = 0x00;

struct extension_introducer {
   uint8_t _introducer_magic;
   uint8_t _extension_label;
};

enum gif_disposal_method : uint8_t {
   kNone = 0,
   kDoNotDispose = 1,
   kRestoreToBackground = 2,
   kRestoreToPrevious = 3,
   kReserved4 = 4,
   kReserved5 = 5,
   kReserved6 = 6,
   kReserved7 = 7,
};

struct application_extension {
   uint8_t _block_size;
   char _application_identifier[8];
   char _authentication_code[3];
};

struct graphics_control_extension {
   uint8_t _reserved_0 : 3;
   gif_disposal_method _disposal_method : 3;
   bool _user_input : 1;
   bool _transparent_enabled : 1;
   uint16_t _delay_time;
   uint8_t _transparent_index;
};

struct plaintext_extension {
   uint8_t _block_size;
   uint16_t _text_left_pos;
   uint16_t _text_top_pos;
   uint16_t _text_width;
   uint16_t _text_height;
   uint8_t _char_cell_width;
   uint8_t _char_cell_height;
   uint8_t _text_fg_color_index;
   uint8_t _text_bg_color_index;
};

struct netscape_extension {
   uint16_t _loop_count;
};

struct image_descriptor {
   uint16_t _image_left_pos;
   uint16_t _image_top_pos;
   uint16_t _image_width;
   uint16_t _image_height;
   bool _lct_present : 1;
   bool _interlaced : 1;
   bool _sorted : 1;
   uint8_t _reserved_1 : 2;
   uint8_t _lct_size : 3;
};
#pragma pack(pop)

enum gif_parse_result {
   kSuccess,
   kFileNotFound,
   kUnexpectedEof,
   kInvalidHeader,
   kNotSupported,
   kInvalidExtensionLabel,
   kMissingBlockTerminator,
   kInvalidApplicationData,
   kInvalidGCEData,
};

struct gif_frame_context {
   std::optional<graphics_control_extension> _extension;
   image_descriptor _descriptor;
   std::vector<color_table_entry> _local_color_table;
   uint8_t _min_code_size;
   std::streampos _image_data_start;
};

// Structure for managing components of a gif in-memory. All modifications are kept in-memory until explicitly
// written out to disk.
class gif {
private:


   struct deserialized_gif_context {
      gif_version _version;
      logical_screen_descriptor _lsd;

      std::vector<color_table_entry> _global_color_table;

      std::vector<gif_frame_context> _frames;

      std::vector<std::string> _comments;
      std::optional<netscape_extension> _nse;
   };

   std::unique_ptr<deserialized_gif_context> _ctx;
   deserialized_gif_context* _ctx_debug;
   std::optional<graphics_control_extension> _active_gce;

   mutable std::ifstream _raw_file;

   gif_parse_result parse_contents();
   gif_parse_result parse_extension();
   gif_parse_result parse_image_data();

   gif_parse_result parse_application_extension();

   void decode_image(gif_frame_context const& frame_ctx, std::vector<uint8_t>& image_out) const;

public:
   gif();
   gif(gif&& rhs);

   gif(gif const&) = delete;
   gif& operator=(gif const&) = delete;

   gif_parse_result open(std::string_view path);
   gif_parse_result open(std::ifstream&& stream);

   template <typename T>
   void foreach_frame_raw(T&& exec) {
      if (!_ctx) {
         return;
      }
      std::vector<uint8_t> image_data;
      for (gif_frame_context const& frame : _ctx->_frames) {
         decode_image(frame, image_data);
         exec(std::forward<std::vector<uint8_t>>(image_data), frame, _ctx->_global_color_table);
      }
   }
};
}

