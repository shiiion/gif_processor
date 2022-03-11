#include "piximg.hh"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace gifproc {
namespace {
constexpr double kPi = 3.141592653589793238462643;
constexpr double kStart = kPi * 1.2;
constexpr double kEnd = kPi * 1.8;
constexpr int kIterations = 50;
constexpr double kStep = (kEnd - kStart) / kIterations;

double compute_r(double t) {
   const double cost = cos(t);
   const double sint = sin(t);
   return std::sqrt(36.0 / ((4.0 * cost * cost) + (9.0 * sint * sint)));
}
double compute_x(double t) {
   const double cost = cos(t);
   double r = compute_r(t);
   return r * cost;
}
double compute_y(double t) {
   const double sint = sin(t);
   double r = compute_r(t);
   return r * sint;
}
}

piximg::piximg(piximg&& rhs) : _img(std::move(rhs._img)), _w(rhs._w), _h(rhs._h) {}

piximg::piximg(piximg const& rhs) : _img(rhs._img), _w(rhs._w), _h(rhs._h) {}

piximg::piximg(std::size_t w, std::size_t h) : _img(w * h), _w(w), _h(h) {}

piximg& piximg::operator=(piximg&& rhs) {
   _img = std::move(rhs._img);
   _w = rhs._w;
   _h = rhs._h;
   return *this;
}

void piximg::draw_line_h(int64_t x0, int64_t y0, int64_t x1, int64_t y1, pixel color, int thickness) {
   // RIP wikipedia
   int64_t dx = x1 - x0;
   int64_t dy = y1 - y0;
   int64_t xi = 1;
   if (dx < 0) {
      xi = -1;
      dx = -dx;
   }
   int64_t D = 2 * dx - dy;
   int64_t x = x0;

   for (int64_t y = y0; y <= y1; y++) {
      std::size_t idx = static_cast<std::size_t>((y * _w) + x);
      // for (int i = -(thickness / 2); i <= (thickness / 2); i++) {
      //    int64_t sidx = static_cast<int64_t>(idx) + i * _w;
      //    if (sidx >= 0) {
      //       _img[sidx] = color;
      //    }
      // }
      if (D > 0) {
         x += xi;
         for (int64_t y_clr = y; y_clr >= 0; y_clr--) {
            _img[(y_clr * _w) + x] = pixel(0, 0, 0, 0);
         }
         D += 2 * (dx - dy);
      } else {
         D += 2 * dx;
      }
   }
}

void piximg::draw_line_l(int64_t x0, int64_t y0, int64_t x1, int64_t y1, pixel color, int thickness) {
   // RIP wikipedia
   int64_t dx = x1 - x0;
   int64_t dy = y1 - y0;
   int64_t yi = 1;
   if (dy < 0) {
      yi = -1;
      dy = -dy;
   }
   int64_t D = 2 * dy - dx;
   int64_t y = y0;

   for (int64_t x = x0; x <= x1; x++) {
      std::size_t idx = static_cast<std::size_t>((y * _w) + x);
      for (int64_t y_clr = y; y_clr >= 0; y_clr--) {
         _img[(y_clr * _w) + x] = pixel(0, 0, 0, 0);
      }
      // for (int i = -(thickness / 2); i <= (thickness / 2); i++) {
      //    int64_t sidx = static_cast<int64_t>(idx) + i * _w;
      //    if (sidx >= 0) {
      //       _img[sidx] = color;
      //    }
      // }
      if (D > 0) {
         y += yi;
         D += 2 * (dy - dx);
      } else {
         D += 2 * dy;
      }
   }
}

void piximg::draw_line(int64_t x0, int64_t y0, int64_t x1, int64_t y1, pixel color, int thickness) {
   if (abs(y1 - y0) < abs(x1 - x0)) {
      if (x0 > x1) {
         draw_line_l(x1, y1, x0, y0, color, thickness);
      } else {
         draw_line_l(x0, y0, x1, y1, color, thickness);
      }
   } else {
      if (y0 > y1) {
         draw_line_h(x1, y1, x0, y0, color, thickness);
      } else {
         draw_line_h(x0, y0, x1, y1, color, thickness);
      }
   }
}

void piximg::expand(std::size_t top) {
   if (top > 0) {
      std::size_t old_sz = _img.size();
      _img.resize(_img.size() + (top * _w));
      std::copy_backward(_img.begin(), _img.begin() + old_sz, _img.end());
      std::fill(_img.begin(), _img.begin() + (top * _w), pixel());
   }
}

void piximg::add_speech_bubble_to_top(int thickness) {
   const double base_width = compute_x(kEnd) - compute_x(kStart);
   const double hw = _w / 2.0;
   const double ratio = _w / base_width;
   const double iy = compute_y(kStart) * ratio;
   constexpr double kStemHeight = 0.2;
   for (int i = 1; i < kIterations + 1; i++) {
      if (i == ((kIterations * 5) / 8)) {
         const double pt = kStart + (i - 1) * kStep;
         const double t = kStart + (i + 3) * kStep;
         const double px = compute_x(pt) * ratio + hw, py = -(compute_y(pt) * ratio - iy);
         const double x = compute_x(t) * ratio + hw, y = -(compute_y(t) * ratio - iy);
         
         const double mx = px;
         const double my = py + (kStemHeight * ratio);

         std::size_t x_p = std::min(_w - 1, std::max(std::size_t{0}, static_cast<std::size_t>(x + 0.5)));
         std::size_t y_p = std::min(_h - 1, std::max(std::size_t{0}, static_cast<std::size_t>(y + 0.5)));
         std::size_t px_p = std::min(_w - 1, std::max(std::size_t{0}, static_cast<std::size_t>(px + 0.5)));
         std::size_t py_p = std::min(_h - 1, std::max(std::size_t{0}, static_cast<std::size_t>(py + 0.5)));
         std::size_t mx_p = std::min(_w - 1, std::max(std::size_t{0}, static_cast<std::size_t>(mx + 0.5)));
         std::size_t my_p = std::min(_h - 1, std::max(std::size_t{0}, static_cast<std::size_t>(my + 0.5)));
         draw_line(mx_p, my_p, x_p, y_p, pixel(0, 0, 0, 255), thickness);
         for (std::size_t i = py_p - (thickness / 2); i < my_p; i++) {
            std::size_t idx = (i * _w) + px_p;
            for (int i = 0; i < thickness; i++) {
               _img[idx + i] = pixel(0, 0, 0, 255);
            }
         }
         i += 3;
      } else {
         const double pt = kStart + (i - 1) * kStep;
         const double t = kStart + i * kStep;
         const double px = compute_x(pt) * ratio + hw, py = -(compute_y(pt) * ratio - iy);
         const double x = compute_x(t) * ratio + hw, y = -(compute_y(t) * ratio - iy);

         std::size_t x_p = std::min(_w - 1, std::max(std::size_t{0}, static_cast<std::size_t>(x + 0.5)));
         std::size_t y_p = std::min(_h - 1, std::max(std::size_t{0}, static_cast<std::size_t>(y + 0.5)));
         std::size_t px_p = std::min(_w - 1, std::max(std::size_t{0}, static_cast<std::size_t>(px + 0.5)));
         std::size_t py_p = std::min(_h - 1, std::max(std::size_t{0}, static_cast<std::size_t>(py + 0.5)));
         draw_line(px_p, py_p, x_p, y_p, pixel(0, 0, 0, 255), thickness);
      }
   }
}

void piximg::dump_to(std::string_view path) const {
   std::cout << "Dumping raw image size " << _w << "x" << _h << " to " << path << std::endl;
   std::ofstream outf(std::string(path), std::ios::binary | std::ios::trunc);
   outf.write(reinterpret_cast<char const*>(_img.data()), _img.size() * sizeof(pixel));
   outf.close();
}

}
