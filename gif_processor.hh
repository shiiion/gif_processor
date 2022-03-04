#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gif_spec.hh"
#include "quant_base.hh"

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
   kInvalidBlockSize,
};

struct gif_frame_context {
   std::size_t _frame_number;
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

   std::unique_ptr<deserialized_gif_context> _dctx;
   deserialized_gif_context* _ctx_debug;
   std::optional<graphics_control_extension> _active_gce;

   struct serialized_gif_context {
      std::size_t _max_w;
      std::size_t _max_h;
      gif_version _required_version;
   };
   std::unique_ptr<serialized_gif_context> _sctx;

   mutable std::ifstream _raw_ifile;
   mutable std::ofstream _raw_ofile;

   gif_parse_result parse_contents();
   gif_parse_result parse_extension();
   gif_parse_result parse_image_data(std::size_t frame_number);

   gif_parse_result parse_application_extension();

   quant::gif_frame decode_image(gif_frame_context const& frame_ctx, quant::gif_frame const& last_frame) const;
   void apply_disposal_method(gif_frame_context const& frame_ctx,
                              quant::gif_frame const& previous_frame,
                              quant::gif_frame& frame);

public:
   gif();
   gif(gif&& rhs);

   gif(gif const&) = delete;
   gif& operator=(gif const&) = delete;

   gif_parse_result open_read(std::string_view path);
   gif_parse_result open_read(std::ifstream&& stream);

   uint16_t width() const { return _dctx->_lsd._canvas_width; }
   uint16_t height() const { return _dctx->_lsd._canvas_height; }

   // Apply disposal method to each frame_ctx
   template <typename T>
   void foreach_frame(T&& exec) {
      if (!_dctx) {
         return;
      }
      quant::gif_frame last_frame(_dctx->_lsd._canvas_width, _dctx->_lsd._canvas_height, 0, 0,
                                  _dctx->_lsd._canvas_width, _dctx->_lsd._canvas_height);

      for (gif_frame_context const& frame_ctx : _dctx->_frames) {
         quant::gif_frame decode_frame = decode_image(frame_ctx, last_frame);
         exec(decode_frame, frame_ctx, _dctx->_global_color_table);
         if (frame_ctx._extension) {
            if (frame_ctx._extension->_disposal_method == gif_disposal_method::kRestoreToBackground) {
               decode_frame.clear_active();
            }
            if (frame_ctx._extension->_disposal_method != gif_disposal_method::kRestoreToPrevious) {
               last_frame = std::move(decode_frame);
            }
         } else {
            last_frame = std::move(decode_frame);
         }
      }
   }

   // Writing
   void open_write(std::string_view path);
   void add_frame(piximg const& frame, std::optional<uint16_t> delay = std::nullopt);
   void add_frame(quant::qimg const& quant_frame, std::optional<uint16_t> delay = std::nullopt);
   void finish_write();
   void finish_write(std::vector<color_table_entry> const& gct);
};
}

