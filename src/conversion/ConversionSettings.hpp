#pragma once

namespace mtb::conversion {

struct ConversionSettings {
  double min_brush_thickness = 8.0;
  double vertex_epsilon = 0.125;
  double coplanar_epsilon = 0.01;
  double normal_epsilon = 0.001;
  double min_face_area = 0.01;
  double brush_plan_chord_error = 8.0;
  int brush_plan_max_depth = 4;
  bool preserve_patches = false;
  bool replace_patches = true;
};

}  // namespace mtb::conversion
