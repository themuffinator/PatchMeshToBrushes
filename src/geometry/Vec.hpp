#pragma once

#include <cmath>

namespace mtb::geometry {

struct Vec2 {
  double u = 0.0;
  double v = 0.0;
};

struct Vec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

inline Vec2 operator+(const Vec2& lhs, const Vec2& rhs) {
  return {lhs.u + rhs.u, lhs.v + rhs.v};
}

inline Vec2 operator-(const Vec2& lhs, const Vec2& rhs) {
  return {lhs.u - rhs.u, lhs.v - rhs.v};
}

inline Vec2 operator*(const Vec2& value, double scale) {
  return {value.u * scale, value.v * scale};
}

inline Vec2 operator/(const Vec2& value, double scale) {
  return {value.u / scale, value.v / scale};
}

inline Vec3 operator+(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

inline Vec3 operator-(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

inline Vec3 operator*(const Vec3& value, double scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

inline Vec3 operator*(double scale, const Vec3& value) {
  return value * scale;
}

inline Vec3 operator/(const Vec3& value, double scale) {
  return {value.x / scale, value.y / scale, value.z / scale};
}

inline double dot(const Vec2& lhs, const Vec2& rhs) {
  return lhs.u * rhs.u + lhs.v * rhs.v;
}

inline double dot(const Vec3& lhs, const Vec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

inline double length_squared(const Vec2& value) {
  return dot(value, value);
}

inline double length_squared(const Vec3& value) {
  return dot(value, value);
}

inline double length(const Vec2& value) {
  return std::sqrt(length_squared(value));
}

inline double length(const Vec3& value) {
  return std::sqrt(length_squared(value));
}

inline double distance(const Vec3& lhs, const Vec3& rhs) {
  return length(lhs - rhs);
}

inline Vec3 normalized(const Vec3& value) {
  const double size = length(value);
  if (size == 0.0) {
    return {};
  }

  return value * (1.0 / size);
}

inline bool is_finite(const Vec2& value) {
  return std::isfinite(value.u) && std::isfinite(value.v);
}

inline bool is_finite(const Vec3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

}  // namespace mtb::geometry
