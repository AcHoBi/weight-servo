#pragma once

float lerp(float a, float b, float t) {
  return (1.0f - t) * a + b * t;
}

float inv_lerp(float a, float b, float v) {
  return (v - a) / (b - a);
}

template<typename T> T clamp(T v, T min, T max) {
  return v < min ? min : v > max ? max : v;
}

float remap(float imin, float imax, float omin, float omax, float v) {
  float t = inv_lerp(imin, imax, v);
  return lerp(omin, omax, t);
}

float remap_clamped(float imin, float imax, float omin, float omax, float i) {
  float t = inv_lerp(imin, imax, i);
  float o = lerp(omin, omax, t);
  return clamp(o, omin, omax);
}