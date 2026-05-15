#pragma once

namespace mtb::conversion {

struct ConversionSettings {
  double min_brush_thickness = 8.0;
  double vertex_epsilon = 0.125;
  double coplanar_epsilon = 0.01;
  double normal_epsilon = 0.001;
  bool preserve_patches = true;
  bool replace_patches = false;
};

}  // namespace mtb::conversion
