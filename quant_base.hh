#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "gif_spec.hh"
#include "piximg.hh"

namespace gifproc::quant {

// Extension of piximg that holds metadata for quantization and dequantization
struct gif_frame : public gifproc::piximg {
   gif_frame(gif_frame&& rhs);
   gif_frame(gif_frame const& rhs);
   gif_frame(std::size_t w, std::size_t h, std::size_t rx, std::size_t ry, std::size_t rw, std::size_t rh);
   gif_frame& operator=(gif_frame&& rhs);

   uint16_t _region_x;
   uint16_t _region_y;
   uint16_t _region_w;
   uint16_t _region_h;

   void clear_region(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
   void clear_active();
};

struct qimg {
   std::vector<uint8_t> _index;
   std::vector<color_table_entry> _palette;
   std::size_t _bpp;
   std::size_t _nbits;
   uint16_t _x;
   uint16_t _y;
   uint16_t _w;
   uint16_t _h;
   std::optional<uint8_t> _t_index;

   qimg(std::vector<uint8_t> index, std::vector<color_table_entry> palette, std::size_t bpp, std::size_t nbits,
        uint16_t x, uint16_t y, uint16_t w, uint16_t h, std::optional<uint8_t> t_index); 
   qimg(std::vector<uint8_t>&& index, std::size_t bpp, std::size_t nbits, uint16_t x, uint16_t y, uint16_t w,
        uint16_t h, std::optional<uint8_t> t_index);
   qimg();
};

}
