#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "gif_spec.hh"
#include "quant_base.hh"

namespace gifproc::quant {

struct dequant_params {
   constexpr dequant_params(bool interlaced, std::optional<gif_disposal_method> disp = std::nullopt)
         : _interlaced(interlaced),
           _disp(disp) {}

   bool _interlaced;
   std::optional<gif_disposal_method> _disp;
};

void prepare_frame(gif_frame& dq_out, dequant_params const& param, gif_frame const& previous);
void dequantize_from(gif_frame& dq_out, dequant_params const& param, qimg const& source);

}
