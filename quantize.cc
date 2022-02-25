#include "quantize.hh"

#include <cstdint>
#include <vector>

#include "bitstream.hh"

namespace gifproc::quant {

constexpr std::size_t kBytesPerPixel = 4;

image::image(uint16_t canvas_w, uint16_t canvas_h, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
      : _image_data(), _canvas_w(canvas_w), _canvas_h(canvas_h), _x(x), _y(y), _w(w), _h(h) {}

image::image(image&& rhs)
      : _image_data(std::move(rhs._image_data)),
        _canvas_w(rhs._canvas_w),
        _canvas_h(rhs._canvas_h),
        _x(rhs._x),
        _y(rhs._y),
        _w(rhs._w),
        _h(rhs._h) {}

image& image::operator=(image&& rhs) {
   _image_data = std::move(rhs._image_data);
   _canvas_w = std::move(rhs._canvas_w);
   _canvas_h = std::move(rhs._canvas_h);
   _x = std::move(rhs._x);
   _y = std::move(rhs._y);
   _w = std::move(rhs._w);
   _h = std::move(rhs._h);
   return *this;
}

constexpr bool in_range(std::size_t v, std::size_t l, std::size_t r) {
   return (v >= l) && (v <= r);
}

template <std::size_t _Bits>
void dequantize_single(dequant_params const& param, util::cbw_istream<_Bits>& source, image& img_out,
                       std::size_t x, std::size_t y) {

   std::size_t pixel_off = ((y * img_out._canvas_w) + x) * kBytesPerPixel;
   uint8_t color_index;
   source >> color_index;

   // If the pixel is transparent, let prepare_frame assign the color
   if (!param._transparency_index || color_index != param._transparency_index) {
      img_out._image_data[pixel_off + 0] = param._color_table[color_index]._red;
      img_out._image_data[pixel_off + 1] = param._color_table[color_index]._green;
      img_out._image_data[pixel_off + 2] = param._color_table[color_index]._blue;
      img_out._image_data[pixel_off + 3] = 255;
   }
}

template <std::size_t _Bits>
void dequantize_image(dequant_params const& param, util::cbw_istream<_Bits>& source, image& img_out) {
   if (param._interlaced) {
      for (std::size_t i = img_out._y; i < img_out._y + img_out._h; i += 8) {
         for (std::size_t j = img_out._x; j < img_out._x + img_out._w; j++) {
            dequantize_single(param, source, img_out, j, i);
         }
      }
      for (std::size_t i = img_out._y + 4; i < img_out._y + img_out._h; i += 8) {
         for (std::size_t j = img_out._x; j < img_out._x + img_out._w; j++) {
            dequantize_single(param, source, img_out, j, i);
         }
      }
      for (std::size_t i = img_out._y + 2; i < img_out._y + img_out._h; i += 4) {
         for (std::size_t j = img_out._x; j < img_out._x + img_out._w; j++) {
            dequantize_single(param, source, img_out, j, i);
         }
      }
      for (std::size_t i = img_out._y + 1; i < img_out._y + img_out._h; i += 2) {
         for (std::size_t j = img_out._x; j < img_out._x + img_out._w; j++) {
            dequantize_single(param, source, img_out, j, i);
         }
      }
   } else {
      // Non-interlaced
      for (std::size_t i = img_out._y; i < img_out._y + img_out._h; i++) {
         for (std::size_t j = img_out._x; j < img_out._x + img_out._w; j++) {
            dequantize_single(param, source, img_out, j, i);
         }
      }
   }
}

void image::clear_region(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
   for (uint32_t i = 0; i < h; i++) {
      const std::size_t row_start = (((i + y) * _canvas_w) + x) * 4;
      const std::size_t row_end = row_start + (w * 4);

      std::fill(_image_data.begin() + row_start, _image_data.begin() + row_end, 0);
   }
}

void image::clear_active() {
   clear_region(_x, _y, _w, _h);
}

void image::prepare_frame_bg() {
   _image_data.resize(_canvas_w * _canvas_h * kBytesPerPixel);
   clear_region(0, 0, _canvas_w, _canvas_h);
}

void image::prepare_frame(dequant_params const& param, image const& previous) {
   _image_data.resize(_canvas_w * _canvas_h * kBytesPerPixel);
   if (!param._disp || param._disp == gif_disposal_method::kNone) {
      prepare_frame_bg();
      return;
   }
   switch (*param._disp) {
      case gif_disposal_method::kRestoreToPrevious:
      case gif_disposal_method::kDoNotDispose:
      case gif_disposal_method::kRestoreToBackground:
         std::copy(previous._image_data.begin(), previous._image_data.end(), _image_data.begin());
         return;
      default:
         return;
   }
}

void image::dequantize_from(dequant_params const& param, std::vector<uint8_t> const& source, std::size_t nbits) {
   switch (param._bpp) {
      case 1: {
         util::cbw_istream<1> stream(source, nbits);
         dequantize_image<1>(param, stream, *this);
         return;
      }
      case 2: {
         util::cbw_istream<2> stream(source, nbits);
         dequantize_image<2>(param, stream, *this);
         return;
      }
      case 3: {
         util::cbw_istream<3> stream(source, nbits);
         dequantize_image<3>(param, stream, *this);
         return;
      }
      case 4: {
         util::cbw_istream<4> stream(source, nbits);
         dequantize_image<4>(param, stream, *this);
         return;
      }
      case 5: {
         util::cbw_istream<5> stream(source, nbits);
         dequantize_image<5>(param, stream, *this);
         return;
      }
      case 6: {
         util::cbw_istream<6> stream(source, nbits);
         dequantize_image<6>(param, stream, *this);
         return;
      }
      case 7: {
         util::cbw_istream<7> stream(source, nbits);
         dequantize_image<7>(param, stream, *this);
         return;
      }
      case 8: {
         util::cbw_istream<8> stream(source, nbits);
         dequantize_image<8>(param, stream, *this);
         return;         
      }
      default:
         assert(false);
   }
}
}
