#include "quantize.hh"

#include <cstdint>
#include <vector>

#include "bitstream.hh"

namespace gifproc::quant {

constexpr std::size_t kBytesPerPixel = 4;

image::image(std::vector<uint8_t>&& image_data, uint16_t width, uint16_t height)
      : _image_data(std::move(image_data)), _width(width), _height(height) {}

constexpr bool in_range(std::size_t v, std::size_t l, std::size_t r) {
   return (v >= l) && (v <= r);
}

template <std::size_t _Bits>
void dequantize_single(dequant_params const& param, util::cbw_istream<_Bits>& source, std::vector<uint8_t>& img,
                       std::size_t x, std::size_t y) {

   std::size_t pixel_off = ((y * param._canvas_w) + x) * kBytesPerPixel;
   uint8_t color_index;
   if (in_range(y, param._y, param._y + param._h - 1) &&
       in_range(x, param._x, param._x + param._w - 1)) {
      source >> color_index;
   } else {
      color_index = param._background_index;
   }

   img[pixel_off + 0] = param._color_table[color_index]._red;
   img[pixel_off + 1] = param._color_table[color_index]._green;
   img[pixel_off + 2] = param._color_table[color_index]._blue;
   if (param._transparency_index && color_index == param._transparency_index) {
      img[pixel_off + 3] = 0;
   } else {
      img[pixel_off + 3] = 255;
   }
}

template <std::size_t _Bits>
image dequantize_image(dequant_params const& param, util::cbw_istream<_Bits>& source) {
   // Pre-allocate our image
   std::vector<uint8_t> image_bytes;
   image_bytes.resize(param._canvas_w * param._canvas_h * 4);

   if (param._interlaced) {
      // Inefficient, bad, slow
      for (std::size_t i = 0; i < param._canvas_h; i++) {
         for (std::size_t j = 0; j < param._canvas_w; j++) {
            if (!in_range(i, param._y, param._y + param._h - 1) ||
                !in_range(j, param._x, param._x + param._w - 1)) {
               dequantize_single(param, source, image_bytes, j, i);
            }
         }
      }
      for (std::size_t i = param._y; i < param._y + param._h; i += 8) {
         for (std::size_t j = param._x; j < param._x + param._w; j++) {
            dequantize_single(param, source, image_bytes, j, i);
         }
      }
      for (std::size_t i = param._y + 4; i < param._y + param._h; i += 8) {
         for (std::size_t j = param._x; j < param._x + param._w; j++) {
            dequantize_single(param, source, image_bytes, j, i);
         }
      }
      for (std::size_t i = param._y + 2; i < param._y + param._h; i += 4) {
         for (std::size_t j = param._x; j < param._x + param._w; j++) {
            dequantize_single(param, source, image_bytes, j, i);
         }
      }
      for (std::size_t i = param._y + 1; i < param._y + param._h; i += 2) {
         for (std::size_t j = param._x; j < param._x + param._w; j++) {
            dequantize_single(param, source, image_bytes, j, i);
         }
      }
   } else {
      // Non-interlaced
      for (std::size_t i = 0; i < param._canvas_h; i++) {
         for (std::size_t j = 0; j < param._canvas_w; j++) {
            dequantize_single(param, source, image_bytes, j, i);
         }
      }
   }

   return image(std::move(image_bytes), param._w, param._h);
}

image dequantize_image(dequant_params const& param, std::vector<uint8_t> const& source, std::size_t nbits) {
   switch (param._bpp) {
      case 1: {
         util::cbw_istream<1> stream(source, nbits);
         return dequantize_image<1>(param, stream);
      }
      case 2: {
         util::cbw_istream<2> stream(source, nbits);
         return dequantize_image<2>(param, stream);
      }
      case 3: {
         util::cbw_istream<3> stream(source, nbits);
         return dequantize_image<3>(param, stream);
      }
      case 4: {
         util::cbw_istream<4> stream(source, nbits);
         return dequantize_image<4>(param, stream);
      }
      case 5: {
         util::cbw_istream<5> stream(source, nbits);
         return dequantize_image<5>(param, stream);
      }
      case 6: {
         util::cbw_istream<6> stream(source, nbits);
         return dequantize_image<6>(param, stream);
      }
      case 7: {
         util::cbw_istream<7> stream(source, nbits);
         return dequantize_image<7>(param, stream);
      }
      case 8: {
         util::cbw_istream<8> stream(source, nbits);
         return dequantize_image<8>(param, stream);
      }
      default:
         assert(false);
         return image({}, 0, 0);
   }
}

}
