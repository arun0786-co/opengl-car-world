// textures.h
//
// procedural texture generation for building surfaces.
//
// my thinking here:
//   instead of loading external png files, i generate pixel data
//   directly in code. this means i dont need any image loading
//   library and the textures are always available.
//
//   each function creates a small 64x64 RGBA pixel grid and uploads
//   it to the gpu as a GL texture. the patterns are deliberately
//   simple (minecraft/cartoonish level) because:
//     1) they tile well with GL_REPEAT
//     2) they are clearly recognizable (brick looks like brick, etc)
//     3) no external files needed
//
//   the concept: i fill a 2d array of (r,g,b,a) bytes.
//   brick pattern: draw horizontal and vertical mortar lines,
//     offset every other row by half a brick (running bond).
//   wood pattern: horizontal plank gaps with grain noise.
//   concrete: uniform grey with random speckle.
//   glass: blue panels separated by dark frame lines.
//   sandstone: warm beige with horizontal sediment layers.
//
//   after filling the pixel array, i call upload_texture() which:
//     glGenTextures  -> creates a texture handle on the gpu
//     glTexImage2D   -> copies the pixel array into gpu memory
//     glTexParameteri -> sets filtering to GL_NEAREST (blocky look)
//                       and wrapping to GL_REPEAT (tiles seamlessly)
//
//   the returned GLuint is the texture id i can bind later with
//   glBindTexture(GL_TEXTURE_2D, id) before drawing a building.

#ifndef TEXTURES_H
#define TEXTURES_H

#include <GL/glew.h>
#include <cstdlib>

// ---- helper: takes raw pixel data and creates a gpu texture ----

static GLuint upload_texture(unsigned char *data, int w, int h)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // upload pixel data to the gpu
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    // GL_NEAREST gives a crisp, blocky, minecraft-like look.
    // if i wanted smooth blending i would use GL_LINEAR instead.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // GL_REPEAT means if UV goes past 1.0 it wraps around,
    // so the pattern tiles seamlessly on larger surfaces.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return tex;
}

// ---- brick texture ----
// running bond pattern: rows of red-brown rectangles with
// grey mortar lines. every other row is offset by half a brick.
// this is the most classic wall pattern.

static GLuint generate_brick_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    int brick_h = 8;   // height of one brick in pixels
    int brick_w = 16;  // width of one brick in pixels
    int mortar  = 1;   // mortar line thickness in pixels

    for (int y = 0; y < H; y++)
    {
        int row = y / brick_h;
        int y_in_brick = y % brick_h;
        bool is_mortar_h = (y_in_brick < mortar);

        // running bond: offset every other row by half a brick
        int x_offset = (row % 2 == 0) ? 0 : brick_w / 2;

        for (int x = 0; x < W; x++)
        {
            int xr = (x + x_offset) % brick_w;
            bool is_mortar_v = (xr < mortar);

            int idx = (y * W + x) * 4;

            if (is_mortar_h || is_mortar_v)
            {
                // mortar: light grey cement color
                data[idx + 0] = 180;
                data[idx + 1] = 175;
                data[idx + 2] = 165;
            }
            else
            {
                // brick: warm red-brown with slight per-pixel noise
                // the noise makes each brick look slightly different
                int noise = (rand() % 20) - 10;
                data[idx + 0] = (unsigned char)(170 + noise);
                data[idx + 1] = (unsigned char)(75 + noise);
                data[idx + 2] = (unsigned char)(55 + noise);
            }
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- wood texture ----
// horizontal planks with dark gaps between them.
// each plank has a slightly different shade for variety.
// grain is simulated with per-pixel noise based on x position.

static GLuint generate_wood_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    int plank_h = 10; // height of each plank in pixels

    for (int y = 0; y < H; y++)
    {
        int plank = y / plank_h;
        int y_in_plank = y % plank_h;
        bool is_gap = (y_in_plank == 0);

        // each plank gets a slightly different brown tone
        int plank_tone = (plank * 37) % 30;

        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;

            if (is_gap)
            {
                // dark gap between planks
                data[idx + 0] = 60;
                data[idx + 1] = 35;
                data[idx + 2] = 20;
            }
            else
            {
                // wood grain: horizontal noise pattern
                int grain = ((x * 7 + y * 3) % 15) - 7;
                data[idx + 0] = (unsigned char)(140 + plank_tone + grain);
                data[idx + 1] = (unsigned char)(95 + plank_tone / 2 + grain);
                data[idx + 2] = (unsigned char)(50 + plank_tone / 3 + grain);
            }
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- concrete texture ----
// uniform grey with random speckle noise.
// looks like bare concrete or plaster.

static GLuint generate_concrete_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;
            int noise = (rand() % 16) - 8;
            unsigned char base = (unsigned char)(155 + noise);
            data[idx + 0] = base;
            data[idx + 1] = (unsigned char)(base - 2);
            data[idx + 2] = (unsigned char)(base + 3);
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- glass panel texture ----
// blue-tinted rectangular panels with dark frame lines.
// gives a modern commercial building look.

static GLuint generate_glass_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    int panel_size = 16; // size of each glass panel in pixels
    int frame = 1;       // frame line thickness

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;

            int xp = x % panel_size;
            int yp = y % panel_size;
            bool is_frame = (xp < frame || yp < frame);

            if (is_frame)
            {
                // dark metal frame
                data[idx + 0] = 40;
                data[idx + 1] = 50;
                data[idx + 2] = 60;
            }
            else
            {
                // blue-ish reflective glass with slight variation
                int noise = (rand() % 10) - 5;
                data[idx + 0] = (unsigned char)(100 + noise);
                data[idx + 1] = (unsigned char)(140 + noise);
                data[idx + 2] = (unsigned char)(190 + noise);
            }
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- sandstone texture ----
// warm beige/tan with horizontal sediment layers.
// the layers are subtle color shifts every few rows.

static GLuint generate_sandstone_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;
            int noise = (rand() % 20) - 10;
            // layer effect: every 4 rows shift tone slightly
            int layer = (y / 4) % 3;
            int layer_offset = layer * 5;
            data[idx + 0] = (unsigned char)(210 + noise - layer_offset);
            data[idx + 1] = (unsigned char)(190 + noise - layer_offset);
            data[idx + 2] = (unsigned char)(150 + noise - layer_offset);
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- grass texture ----
// green patches with slight variation to look like a grass field.
// darker and lighter blades alternate for a natural look.

static GLuint generate_grass_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;
            int noise = (rand() % 30) - 15;
            // blades of grass: vertical streaks based on x
            int blade = ((x * 13 + y * 7) % 20) - 10;
            data[idx + 0] = (unsigned char)(55 + noise + blade / 2);
            data[idx + 1] = (unsigned char)(120 + noise + blade);
            data[idx + 2] = (unsigned char)(45 + noise + blade / 3);
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- asphalt texture ----
// dark grey with gravel speckle noise and occasional lighter pebbles.
// looks like a real tar road surface.

static GLuint generate_asphalt_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;
            int noise = (rand() % 18) - 9;
            // occasional lighter pebble
            bool pebble = (rand() % 40) == 0;
            int base = pebble ? 90 : 52;
            data[idx + 0] = (unsigned char)(base + noise);
            data[idx + 1] = (unsigned char)(base + noise);
            data[idx + 2] = (unsigned char)(base + noise + 2);
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- metallic car paint texture ----
// a smooth gradient with subtle horizontal streaks that simulate
// the reflective look of car body paint.

static GLuint generate_metal_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;
            // horizontal reflection streaks
            int streak = ((y * 3 + x / 4) % 12) - 6;
            int noise = (rand() % 8) - 4;
            // neutral grey base — the actual color comes from the tint
            data[idx + 0] = (unsigned char)(200 + streak + noise);
            data[idx + 1] = (unsigned char)(200 + streak + noise);
            data[idx + 2] = (unsigned char)(205 + streak + noise);
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- leaf/foliage texture ----
// clusters of light and dark green with occasional yellow spots
// to look like a blocky canopy of leaves.

static GLuint generate_leaf_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 4;
            int noise = (rand() % 40) - 20;
            // cluster pattern: groups of 4x4 pixels get similar shade
            int cluster = ((x / 4) * 17 + (y / 4) * 31) % 30;
            data[idx + 0] = (unsigned char)(40 + cluster / 2 + noise / 3);
            data[idx + 1] = (unsigned char)(110 + cluster + noise);
            data[idx + 2] = (unsigned char)(30 + cluster / 3 + noise / 4);
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

// ---- stone wall texture ----
// stacked rectangular stone blocks with dark mortar lines.
// similar to brick but with larger, more irregular blocks
// and a grey/brown stone color instead of red.

static GLuint generate_stone_texture()
{
    const int W = 64, H = 64;
    unsigned char data[W * H * 4];

    int block_h = 10;
    int block_w = 20;
    int mortar = 1;

    for (int y = 0; y < H; y++)
    {
        int row = y / block_h;
        int y_in_block = y % block_h;
        bool is_mortar_h = (y_in_block < mortar);

        int x_offset = (row % 2 == 0) ? 0 : block_w / 3;

        for (int x = 0; x < W; x++)
        {
            int xr = (x + x_offset) % block_w;
            bool is_mortar_v = (xr < mortar);

            int idx = (y * W + x) * 4;

            if (is_mortar_h || is_mortar_v)
            {
                data[idx + 0] = 80;
                data[idx + 1] = 75;
                data[idx + 2] = 70;
            }
            else
            {
                int noise = (rand() % 16) - 8;
                data[idx + 0] = (unsigned char)(130 + noise);
                data[idx + 1] = (unsigned char)(120 + noise);
                data[idx + 2] = (unsigned char)(105 + noise);
            }
            data[idx + 3] = 255;
        }
    }

    return upload_texture(data, W, H);
}

#endif // TEXTURES_H
