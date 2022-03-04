#pragma once

#include <cstdint>
#include <libimagequant.h>
#include <vector>

#include "gif_spec.hh"
#include "quant_base.hh"

// Shim for libimagequant

namespace gifproc::quant {

struct multi_quant_ctx {
   liq_attr* _attr;
   liq_histogram* _histogram;
   std::vector<std::pair<liq_image*, uint16_t>> _frames;
   std::vector<color_table_entry> _palette;
   liq_result* _result;
   uint16_t _w;
   uint16_t _h;
   std::optional<uint8_t> _t_index;
   uint8_t _bpp;
};

void quantize(piximg const& img, qimg& q_out);

multi_quant_ctx begin_quantize_multiple(uint16_t w, uint16_t h);
void step_quantize_multiple(piximg const& img, multi_quant_ctx& ctx, uint16_t delay);
void end_quantize_multiple(multi_quant_ctx& ctx);

template <typename T>
void foreach_quantize_multi(multi_quant_ctx& ctx, T&& exec) {
   qimg cur_img;
   cur_img._index.resize(ctx._w * ctx._h);
   cur_img._t_index = ctx._t_index;
   cur_img._bpp = ctx._bpp;
   cur_img._nbits = cur_img._index.size() * ctx._bpp;
   cur_img._w = ctx._w;
   cur_img._h = ctx._h;
   cur_img._x = cur_img._y = 0;
   for (auto&& [raw_img, delay] : ctx._frames) {
      cur_img._palette.clear();
      liq_write_remapped_image(ctx._result, raw_img, cur_img._index.data(), cur_img._index.size());
      exec(cur_img, delay);
   }
}

}
