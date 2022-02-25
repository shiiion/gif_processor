#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "gif_spec.hh"

namespace gifproc::quant {

struct dequant_params {
   constexpr dequant_params(std::vector<color_table_entry> const& color_table, uint8_t bpp, bool interlaced,
                            std::optional<gif_disposal_method> disp = std::nullopt,
                            std::optional<uint8_t> transparency_index = std::nullopt)
         : _color_table(color_table),
           _bpp(bpp),
           _interlaced(interlaced),
           _disp(disp),
           _transparency_index(transparency_index) {}

   std::vector<color_table_entry> const& _color_table;
   uint8_t _bpp;
   bool _interlaced;
   std::optional<gif_disposal_method> _disp;
   std::optional<uint8_t> _transparency_index;
};

struct image {
   image(uint16_t canvas_w, uint16_t canvas_h, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
   image(image&& rhs);
   image& operator=(image&& rhs);

   std::vector<uint8_t> _image_data;
   uint16_t _canvas_w;
   uint16_t _canvas_h;
   uint16_t _x;
   uint16_t _y;
   uint16_t _w;
   uint16_t _h;

   void clear_region(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
   void clear_active();
   // Before-initial frame initialized
   void prepare_frame_bg();
   void prepare_frame(dequant_params const& param, image const& previous);
   void dequantize_from(dequant_params const& param, std::vector<uint8_t> const& source, std::size_t nbits);
};

void quantize_image();

}
