#pragma once

#include <cstdint>

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

   uint8_t _gct_size : 3;
   bool _sort_flag : 1;
   uint8_t _color_resolution : 3;
   bool _gct_present : 1;

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
   char _application_identifier[8];
   char _authentication_code[3];
};
constexpr std::size_t kApplicationExtensionSize = 11;
static_assert(sizeof(application_extension) == kApplicationExtensionSize);

struct graphics_control_extension {
   bool _transparent_enabled : 1;
   bool _user_input : 1;
   gif_disposal_method _disposal_method : 3;
   uint8_t _reserved_0 : 3;
   uint16_t _delay_time;
   uint8_t _transparent_index;
};
constexpr std::size_t kGraphicsExtensionSize = 4;
static_assert(sizeof(graphics_control_extension) == kGraphicsExtensionSize);

struct plaintext_extension {
   uint16_t _text_left_pos;
   uint16_t _text_top_pos;
   uint16_t _text_width;
   uint16_t _text_height;
   uint8_t _char_cell_width;
   uint8_t _char_cell_height;
   uint8_t _text_fg_color_index;
   uint8_t _text_bg_color_index;
};
constexpr std::size_t kPlaintextExtensionSize = 12;
static_assert(sizeof(plaintext_extension) == kPlaintextExtensionSize);

struct netscape_extension {
   uint16_t _loop_count;
};

struct image_descriptor {
   uint16_t _image_left_pos;
   uint16_t _image_top_pos;
   uint16_t _image_width;
   uint16_t _image_height;
   uint8_t _lct_size : 3;
   uint8_t _reserved_1 : 2;
   bool _sorted : 1;
   bool _interlaced : 1;
   bool _lct_present : 1;
};
#pragma pack(pop)

}
