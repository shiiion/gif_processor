#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace gifproc {

constexpr std::size_t kBytesPerPixel = 4;

struct pixel {
   constexpr pixel(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 0) : _r(r), _g(g), _b(b), _a(a) {}
   uint8_t _r;
   uint8_t _g;
   uint8_t _b;
   uint8_t _a;
};

class piximg {
public:
   std::vector<pixel> _img;
   std::size_t _w;
   std::size_t _h;
   void draw_line_h(int64_t x0, int64_t y0, int64_t x1, int64_t y1, pixel color, int thickness);
   void draw_line_l(int64_t x0, int64_t y0, int64_t x1, int64_t y1, pixel color, int thickness);
   void draw_line(int64_t x0, int64_t y0, int64_t x1, int64_t y1, pixel color, int thickness);

public:
   piximg(piximg&& rhs);
   piximg(piximg const& rhs);
   piximg(std::size_t w, std::size_t h);
   piximg& operator=(piximg&& rhs);

   void expand(std::size_t top);
   void add_speech_bubble_to_top(int thickness);
   void dump_to(std::string_view path) const;
};

}
