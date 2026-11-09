// checks the separating axis test in src/collide.h against a plain
// polygon intersection on random rectangles, plus the two cases the
// old axis aligned checks got wrong.

#include <cstdio>
#include <cstdlib>
#include <random>

#include "../src/collide.h"

static int failures = 0;
#define CHECK(cond)                                                   \
  do                                                                  \
  {                                                                   \
    if (!(cond))                                                      \
    {                                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
      failures++;                                                     \
    }                                                                 \
  } while (0)

struct P
{
  double x, z;
};

static void corners(const Rect2 &r, P out[4])
{
  double a = r.deg * M_PI / 180.0;
  double ux = std::cos(a), uz = std::sin(a), vx = -std::sin(a), vz = std::cos(a);
  const int sx[4] = {1, -1, -1, 1}, sz[4] = {1, 1, -1, -1};
  for (int i = 0; i < 4; i++)
    out[i] = {r.cx + sx[i] * r.hx * ux + sz[i] * r.hz * vx,
              r.cz + sx[i] * r.hx * uz + sz[i] * r.hz * vz};
}

static double cross(P o, P a, P b)
{
  return (a.x - o.x) * (b.z - o.z) - (a.z - o.z) * (b.x - o.x);
}

// strictly inside a convex polygon given in either winding
static bool inside(const P poly[4], P p)
{
  int pos = 0, neg = 0;
  for (int i = 0; i < 4; i++)
  {
    double c = cross(poly[i], poly[(i + 1) % 4], p);
    pos += c > 0;
    neg += c < 0;
  }
  return pos == 4 || neg == 4;
}

static bool segments_cross(P a, P b, P c, P d)
{
  double d1 = cross(c, d, a), d2 = cross(c, d, b);
  double d3 = cross(a, b, c), d4 = cross(a, b, d);
  return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)) && d1 != 0 &&
         d2 != 0 && d3 != 0 && d4 != 0;
}

// reference: convex polygons overlap if an edge pair crosses or one
// contains a corner of the other
static bool reference_overlap(const Rect2 &a, const Rect2 &b)
{
  P pa[4], pb[4];
  corners(a, pa);
  corners(b, pb);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      if (segments_cross(pa[i], pa[(i + 1) % 4], pb[j], pb[(j + 1) % 4]))
        return true;
  for (int i = 0; i < 4; i++)
    if (inside(pa, pb[i]) || inside(pb, pa[i]))
      return true;
  // same center: neither test above fires for identical squares
  return inside(pa, {b.cx, b.cz}) || inside(pb, {a.cx, a.cz});
}

int main()
{
  std::mt19937 rng(7);
  std::uniform_real_distribution<float> pos(-3.0f, 3.0f), half(0.1f, 2.0f),
      ang(-180.0f, 180.0f);
  int agree = 0, hits = 0;
  const int n = 200000;
  for (int i = 0; i < n; i++)
  {
    Rect2 a{pos(rng), pos(rng), half(rng), half(rng), ang(rng)};
    Rect2 b{pos(rng), pos(rng), half(rng), half(rng), ang(rng)};
    bool sat = rects_overlap(a, b), ref = reference_overlap(a, b);
    agree += sat == ref;
    hits += ref;
    CHECK(rects_overlap(a, b) == rects_overlap(b, a));
  }
  std::printf("random pairs: %d of %d agree with the polygon test (%d overlapping)\n",
              agree, n, hits);
  // float rounding can flip pairs that only just touch; allow a handful
  CHECK(n - agree <= 5);

  // car (half length 1, half width 0.5) turned 90 degrees, so it is long
  // along z, with a 1x1 building just in front of its nose. the old test
  // used the half length along x and the half width along z whatever
  // the heading, and missed this.
  Rect2 car{0.0f, 0.0f, 1.0f, 0.5f, 90.0f};
  Rect2 building{0.0f, 1.4f, 0.5f, 0.5f, 0.0f};
  CHECK(rects_overlap(car, building));
  // and a building beside it that the old test reported as a hit
  Rect2 side{1.4f, 0.0f, 0.5f, 0.5f, 0.0f};
  CHECK(!rects_overlap(car, side));

  // the old wall test used max(half length, half width) = 1 as the reach
  // of the car; at 45 degrees a corner reaches 1.06 and went into the wall
  Rect2 diag{1.05f, 20.0f, 1.0f, 0.5f, 45.0f};
  CHECK(rect_outside(diag, 40.0f, 40.0f));
  Rect2 diag_ok{1.1f, 20.0f, 1.0f, 0.5f, 45.0f};
  CHECK(!rect_outside(diag_ok, 40.0f, 40.0f));
  // heading 90: the car reaches 1.0 along z but only 0.5 along x
  Rect2 upright{0.6f, 20.0f, 1.0f, 0.5f, 90.0f};
  CHECK(!rect_outside(upright, 40.0f, 40.0f));

  if (failures)
  {
    std::printf("%d failures\n", failures);
    return 1;
  }
  std::printf("all collision tests passed\n");
  return 0;
}
