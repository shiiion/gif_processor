#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct logical_screen_descriptor {
   uint16_t _canvas_width;
   uint16_t _canvas_height;

   bool _gct_present : 1;
   uint8_t _color_resolution : 3;
   bool _sort_flag : 1 = false;
   uint8_t _gct_size : 3;

   uint8_t _bg_color_index;
   uint8_t _pixel_aspect_ratio = 0;
};

struct gif_header {
   char _version[6];
   logical_screen_descriptor _lsd;
};
#pragma pack(pop)
