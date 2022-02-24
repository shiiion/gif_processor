#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gif_spec.hh"
#include "quantize.hh"

namespace gifproc {

enum gif_parse_result {
   kSuccess,
   kFileNotFound,
   kUnexpectedEof,
   kInvalidHeader,
   kNotSupported,
   kInvalidExtensionLabel,
   kMissingBlockTerminator,
   kInvalidApplicationData,
   kInvalidGCEData,
};

struct gif_frame_context {
   std::optional<graphics_control_extension> _extension;
   image_descriptor _descriptor;
   std::vector<color_table_entry> _local_color_table;
   uint8_t _min_code_size;
   std::streampos _image_data_start;
};

// Structure for managing components of a gif in-memory. All modifications are kept in-memory until explicitly
// written out to disk.
class gif {
private:
   struct deserialized_gif_context {
      gif_version _version;
      logical_screen_descriptor _lsd;

      std::vector<color_table_entry> _global_color_table;

      std::vector<gif_frame_context> _frames;

      std::vector<std::string> _comments;
      std::optional<netscape_extension> _nse;
   };

   std::unique_ptr<deserialized_gif_context> _ctx;
   deserialized_gif_context* _ctx_debug;
   std::optional<graphics_control_extension> _active_gce;

   mutable std::ifstream _raw_file;

   gif_parse_result parse_contents();
   gif_parse_result parse_extension();
   gif_parse_result parse_image_data();

   gif_parse_result parse_application_extension();

   quant::dequant_params dq_params_for_frame(gif_frame_context const& frame) const;
   quant::image decode_image(gif_frame_context const& frame_ctx) const;

public:
   gif();
   gif(gif&& rhs);

   gif(gif const&) = delete;
   gif& operator=(gif const&) = delete;

   gif_parse_result open(std::string_view path);
   gif_parse_result open(std::ifstream&& stream);

   template <typename T>
   void foreach_frame_raw(T&& exec) {
      if (!_ctx) {
         return;
      }
      std::vector<uint8_t> image_data;
      for (gif_frame_context const& frame : _ctx->_frames) {
         quant::image decode_frame = decode_image(frame);
         exec(decode_frame, frame, _ctx->_global_color_table);
      }
   }
};
}

