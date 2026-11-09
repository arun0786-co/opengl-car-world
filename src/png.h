// minimal png writer for frame captures: 8-bit rgb, one idat chunk,
// compressed with zlib. enough for screenshots, nothing more.

#ifndef PNG_H
#define PNG_H

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace png
{

inline void put_u32(std::vector<uint8_t> &out, uint32_t v)
{
  out.push_back(uint8_t(v >> 24));
  out.push_back(uint8_t(v >> 16));
  out.push_back(uint8_t(v >> 8));
  out.push_back(uint8_t(v));
}

// a chunk is length, type, data, then a crc over type and data
inline void put_chunk(std::vector<uint8_t> &out, const char *type,
                      const uint8_t *data, size_t n)
{
  put_u32(out, uint32_t(n));
  size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data, data + n);
  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, out.data() + start, uInt(out.size() - start));
  put_u32(out, uint32_t(crc));
}

// rgb holds h rows of w pixels, top row first
inline bool write_rgb(const char *path, const uint8_t *rgb, int w, int h)
{
  // every row starts with a filter byte; 0 means no filter
  std::vector<uint8_t> raw;
  raw.reserve(size_t(h) * (size_t(w) * 3 + 1));
  for (int y = 0; y < h; y++)
  {
    raw.push_back(0);
    raw.insert(raw.end(), rgb + size_t(y) * w * 3, rgb + size_t(y + 1) * w * 3);
  }
  uLongf zlen = compressBound(uLong(raw.size()));
  std::vector<uint8_t> z(zlen);
  if (compress2(z.data(), &zlen, raw.data(), uLong(raw.size()), 6) != Z_OK)
    return false;

  std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<uint8_t> ihdr;
  put_u32(ihdr, uint32_t(w));
  put_u32(ihdr, uint32_t(h));
  ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0}); // 8 bits, rgb, deflate, no filter, no interlace
  put_chunk(out, "IHDR", ihdr.data(), ihdr.size());
  put_chunk(out, "IDAT", z.data(), zlen);
  put_chunk(out, "IEND", nullptr, 0);

  FILE *f = std::fopen(path, "wb");
  if (!f)
    return false;
  bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
  return std::fclose(f) == 0 && ok;
}

} // namespace png

#endif
