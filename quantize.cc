#include "quantize.hh"

#include <algorithm>
#include <vector>
#include <libimagequant.h>

#include "bitstream.hh"
#include "gif_spec.hh"
#include "quant_base.hh"

namespace gifproc::quant {

void quantize(piximg const& img, qimg& q_out) {
   liq_attr *attr = liq_attr_create();
   liq_image *image = liq_image_create_rgba(attr, img._img.data(), img._w, img._h, 0);
   liq_result *res;
   auto result = liq_image_quantize(image, attr, &res);
   if (result != LIQ_OK) {
      return;
   }

   q_out._index.resize(img._w * img._h);
   q_out._bpp = 8;
   q_out._nbits = util::to_bit(q_out._index.size());
   q_out._w = img._w;
   q_out._h = img._h;
   q_out._x = 0;
   q_out._y = 0;
   liq_write_remapped_image(res, image, q_out._index.data(), q_out._index.size());
   liq_palette const* palette = liq_get_palette(res);

   for (unsigned int i = 0; i < palette->count; i++) {
      if (palette->entries[i].a == 255) {
         q_out._palette.emplace_back(
               color_table_entry {palette->entries[i].r,palette->entries[i].g,palette->entries[i].b});
      } else {
         q_out._palette.emplace_back(color_table_entry {0, 0, 0});
         q_out._t_index = i;
      }
   }
   liq_result_destroy(res);
   liq_image_destroy(image);
   liq_attr_destroy(attr);
}

multi_quant_ctx begin_quantize_multiple(uint16_t w, uint16_t h) {
   multi_quant_ctx ret;
   ret._attr = liq_attr_create();
   ret._histogram = liq_histogram_create(ret._attr);
   ret._result = nullptr;
   ret._w = w;
   ret._h = h;
   ret._t_index = std::nullopt;

   return ret;
}

void step_quantize_multiple(piximg const& img, multi_quant_ctx& ctx, uint16_t delay) {
   liq_image* new_image = liq_image_create_rgba(ctx._attr, img._img.data(), img._w, img._h, 0);
   liq_histogram_add_image(ctx._histogram, ctx._attr, new_image);
   ctx._frames.emplace_back(new_image, delay);
}

void end_quantize_multiple(multi_quant_ctx& ctx) {
   liq_error err = liq_histogram_quantize(ctx._histogram, ctx._attr, &ctx._result);
   if (err != LIQ_OK) {
      ctx._result = nullptr;
   }
   liq_palette const* palette = liq_get_palette(ctx._result);
   for (unsigned int i = 0; i < palette->count; i++) {
      if (palette->entries[i].a == 255) {
         ctx._palette.emplace_back(
               color_table_entry {palette->entries[i].r,palette->entries[i].g,palette->entries[i].b});
      } else {
         ctx._palette.emplace_back(color_table_entry {0, 0, 0});
         ctx._t_index = i;
      }
   }
   ctx._bpp = 8;
}

}
