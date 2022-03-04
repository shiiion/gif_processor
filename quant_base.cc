#include "quant_base.hh"

namespace gifproc::quant {

gif_frame::gif_frame(gif_frame&& rhs)
      : piximg(std::move(rhs)),
        _region_x(rhs._region_x),
        _region_y(rhs._region_y),
        _region_w(rhs._region_w),
        _region_h(rhs._region_h) {}

gif_frame::gif_frame(gif_frame const& rhs)
      : piximg(rhs),
        _region_x(rhs._region_x),
        _region_y(rhs._region_y),
        _region_w(rhs._region_w),
        _region_h(rhs._region_h) {}

gif_frame::gif_frame(std::size_t w, std::size_t h, std::size_t rx, std::size_t ry, std::size_t rw, std::size_t rh)
      : piximg(w, h), _region_x(rx), _region_y(ry), _region_w(rw), _region_h(rh) {}

gif_frame& gif_frame::operator=(gif_frame&& rhs) {
   _img = std::move(rhs._img);
   _w = rhs._w;
   _h = rhs._h;
   _region_x = rhs._region_x;
   _region_y = rhs._region_y;
   _region_w = rhs._region_w;
   _region_h = rhs._region_h;
   return *this;
}

void gif_frame::clear_region(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
   for (uint32_t i = 0; i < h; i++) {
      const std::size_t row_start = ((i + y) * _w) + x;
      const std::size_t row_end = row_start + w;

      std::fill(_img.begin() + row_start, _img.begin() + row_end, pixel());
   }
}

void gif_frame::clear_active() {
   clear_region(_region_x, _region_y, _region_w, _region_h);
}

qimg::qimg(std::vector<uint8_t> index, std::vector<color_table_entry> palette, std::size_t bpp, std::size_t nbits,
           uint16_t x, uint16_t y, uint16_t w, uint16_t h, std::optional<uint8_t> t_index)
      : _index(std::move(index)), _palette(std::move(palette)), _bpp(bpp), _nbits(nbits), _x(x), _y(y), _w(w), _h(h),
        _t_index(t_index) {}

qimg::qimg(std::vector<uint8_t>&& index, std::size_t bpp, std::size_t nbits, uint16_t x, uint16_t y, uint16_t w,
           uint16_t h, std::optional<uint8_t> t_index)
      : _index(std::move(index)), _palette(), _bpp(bpp), _nbits(nbits), _x(x), _y(y), _w(w), _h(h),
        _t_index(t_index) {}

qimg::qimg() : _bpp(0), _nbits(0), _x(0), _y(0), _w(0), _h(0), _t_index(std::nullopt) {}

}
