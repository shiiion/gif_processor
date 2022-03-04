#include "dequantize.hh"

#include <cstdint>
#include <vector>

#include "bitstream.hh"

namespace gifproc::quant {

template <std::size_t _Bits>
void dequantize_single(dequant_params const& param, qimg const& img_meta, util::cbw_istream<_Bits>& source,
                       gif_frame& img_out, std::size_t x, std::size_t y) {

   std::size_t pixel_off = (y * img_out._w) + x;
   uint8_t color_index;
   source >> color_index;

   // If the pixel is transparent, let prepare_frame assign the color
   if (!img_meta._t_index || color_index != img_meta._t_index) {
      img_out._img[pixel_off]._r = img_meta._palette[color_index]._red;
      img_out._img[pixel_off]._g = img_meta._palette[color_index]._green;
      img_out._img[pixel_off]._b = img_meta._palette[color_index]._blue;
      img_out._img[pixel_off]._a = 255;
   }
}

template <std::size_t _Bits>
void dequantize_image(dequant_params const& param, qimg const& source, gif_frame& img_out) {
   util::cbw_istream<_Bits> stream(source._index, source._nbits);
   if (param._interlaced) {
      for (std::size_t i = img_out._region_y; i < img_out._region_y + img_out._region_h; i += 8) {
         for (std::size_t j = img_out._region_x; j < img_out._region_x + img_out._region_w; j++) {
            dequantize_single(param, source, stream, img_out, j, i);
         }
      }
      for (std::size_t i = img_out._region_y + 4; i < img_out._region_y + img_out._region_h; i += 8) {
         for (std::size_t j = img_out._region_x; j < img_out._region_x + img_out._region_w; j++) {
            dequantize_single(param, source, stream, img_out, j, i);
         }
      }
      for (std::size_t i = img_out._region_y + 2; i < img_out._region_y + img_out._region_h; i += 4) {
         for (std::size_t j = img_out._region_x; j < img_out._region_x + img_out._region_w; j++) {
            dequantize_single(param, source, stream, img_out, j, i);
         }
      }
      for (std::size_t i = img_out._region_y + 1; i < img_out._region_y + img_out._region_h; i += 2) {
         for (std::size_t j = img_out._region_x; j < img_out._region_x + img_out._region_w; j++) {
            dequantize_single(param, source, stream, img_out, j, i);
         }
      }
   } else {
      // Non-interlaced
      for (std::size_t i = img_out._region_y; i < img_out._region_y + img_out._region_h; i++) {
         for (std::size_t j = img_out._region_x; j < img_out._region_x + img_out._region_w; j++) {
            dequantize_single(param, source, stream, img_out, j, i);
         }
      }
   }
}

void prepare_frame(gif_frame& dq_out, dequant_params const& param, gif_frame const& previous) {
   dq_out._img.resize(dq_out._w * dq_out._h);
   if (!param._disp || param._disp == gif_disposal_method::kNone) {
      dq_out.clear_region(0, 0, dq_out._w, dq_out._h);
      return;
   }
   switch (*param._disp) {
      case gif_disposal_method::kRestoreToPrevious:
      case gif_disposal_method::kDoNotDispose:
      case gif_disposal_method::kRestoreToBackground:
         std::copy(previous._img.begin(), previous._img.end(), dq_out._img.begin());
         return;
      default:
         return;
   }
}

void dequantize_from(gif_frame& dq_out, dequant_params const& param, qimg const& source) {
   switch (source._bpp) {
      case 1: {
         dequantize_image<1>(param, source, dq_out);
         return;
      }
      case 2: {
         dequantize_image<2>(param, source, dq_out);
         return;
      }
      case 3: {
         dequantize_image<3>(param, source, dq_out);
         return;
      }
      case 4: {
         dequantize_image<4>(param, source, dq_out);
         return;
      }
      case 5: {
         dequantize_image<5>(param, source, dq_out);
         return;
      }
      case 6: {
         dequantize_image<6>(param, source, dq_out);
         return;
      }
      case 7: {
         dequantize_image<7>(param, source, dq_out);
         return;
      }
      case 8: {
         dequantize_image<8>(param, source, dq_out);
         return;         
      }
      default:
         assert(false);
   }
}
}
