#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "gif_spec.hh"

namespace gifproc::quant {

struct dequant_params {
   constexpr dequant_params(std::vector<color_table_entry> const& color_table, uint16_t canvas_w, uint16_t canvas_h,
                            uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t background_index, uint8_t bpp,
                            bool interlaced)
         : _color_table(color_table),
           _canvas_w(canvas_w),
           _canvas_h(canvas_h),
           _x(x),
           _y(y),
           _w(w),
           _h(h),
           _background_index(background_index),
           _interlaced(interlaced),
           _bpp(bpp),
           _transparency_index(std::nullopt) {}

   constexpr dequant_params(std::vector<color_table_entry> const& color_table, uint16_t canvas_w, uint16_t canvas_h,
                            uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t background_index, uint8_t bpp,
                            bool interlaced, uint8_t transparency_index)
         : _color_table(color_table),
           _canvas_w(canvas_w),
           _canvas_h(canvas_h),
           _x(x),
           _y(y),
           _w(w),
           _h(h),
           _background_index(background_index),
           _interlaced(interlaced),
           _bpp(bpp),
           _transparency_index(transparency_index) {}

   std::vector<color_table_entry> const& _color_table;
   uint16_t _canvas_w;
   uint16_t _canvas_h;
   uint16_t _x;
   uint16_t _y;
   uint16_t _w;
   uint16_t _h;
   uint8_t _background_index;
   bool _interlaced;
   uint8_t _bpp;
   std::optional<uint8_t> _transparency_index;
};

struct image {
   image(std::vector<uint8_t>&& image_data, uint16_t width, uint16_t height);

   std::vector<uint8_t> _image_data;
   uint16_t _width;
   uint16_t _height;
};

image dequantize_image(dequant_params const& param, std::vector<uint8_t> const& source, std::size_t nbits);
void quantize_image();

}
