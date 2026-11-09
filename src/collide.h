// collision tests on the ground plane (x, z).
//
// every object is a rectangle seen from above: a center, half sizes
// along its own x and z axes, and a rotation about the vertical. the
// rotation follows the model matrices: at angle h the local x axis
// points along (cos h, sin h) and the local z axis along (-sin h, cos h),
// which is also the direction a car with heading h drives in.
//
// two convex shapes are apart exactly when some axis separates their
// projections (separating axis theorem). for two rectangles it is
// enough to try the 4 edge normals, two from each rectangle.

#ifndef COLLIDE_H
#define COLLIDE_H

#include <cmath>

struct Rect2
{
  float cx, cz;   // center
  float hx, hz;   // half size along the local x and z axes
  float deg = 0;  // rotation about the vertical axis
};

// half widths of the axis aligned box around r
inline void rect_extent(const Rect2 &r, float &ex, float &ez)
{
  float a = r.deg * float(M_PI) / 180.0f;
  float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
  ex = r.hx * c + r.hz * s;
  ez = r.hx * s + r.hz * c;
}

// true if the rectangles overlap (touching edges count as apart)
inline bool rects_overlap(const Rect2 &a, const Rect2 &b)
{
  float aa = a.deg * float(M_PI) / 180.0f, ba = b.deg * float(M_PI) / 180.0f;
  // local x and z axes of both rectangles
  const float ax[2][2] = {{std::cos(aa), std::sin(aa)},
                          {-std::sin(aa), std::cos(aa)}};
  const float bx[2][2] = {{std::cos(ba), std::sin(ba)},
                          {-std::sin(ba), std::cos(ba)}};
  float dx = b.cx - a.cx, dz = b.cz - a.cz;

  const float(*axes[4]) = {ax[0], ax[1], bx[0], bx[1]};
  for (const float *n : axes)
  {
    float dist = std::fabs(dx * n[0] + dz * n[1]);
    float ra = a.hx * std::fabs(ax[0][0] * n[0] + ax[0][1] * n[1]) +
               a.hz * std::fabs(ax[1][0] * n[0] + ax[1][1] * n[1]);
    float rb = b.hx * std::fabs(bx[0][0] * n[0] + bx[0][1] * n[1]) +
               b.hz * std::fabs(bx[1][0] * n[0] + bx[1][1] * n[1]);
    if (dist >= ra + rb)
      return false;
  }
  return true;
}

// true if any part of r lies outside [0, w] x [0, d]
inline bool rect_outside(const Rect2 &r, float w, float d)
{
  float ex, ez;
  rect_extent(r, ex, ez);
  return r.cx - ex < 0.0f || r.cx + ex > w || r.cz - ez < 0.0f ||
         r.cz + ez > d;
}

#endif
