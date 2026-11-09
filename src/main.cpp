
// when i read this file, i want to understand the entire 3d car world
// from my own point of view:
//
//   "how do i build an interactive 3d scene with a drivable car,
//    a wobbly racetrack, textured buildings, spinning windmills,
//    swiveling lights, ai traffic, street lights, a moving sun,
//    trees, humans, and 9 camera views?"
//
// this file is my roadmap. every section tells me:
//   what concept i am using
//   what variables i need to define
//   what functions i must write
//   how it all fits into the game loop
//
// the world is a flat rectangular arena. inside it there is a
// wobbly (distorted oval) track. a player car sits on the track
// and can be driven with keyboard. two ai cars drive constantly
// in opposite directions on either lane. on the sides of the
// track there are 5 buildings (1, 2, or 3 stories) each with:
//   a texture mapped surface (brick, wood, concrete, glass, sandstone)
//   windows and doors for visual detail
//   a windmill fan (white blades + black nacelle box)
//   a swiveling gimbal light (elevated, offset to front)
// the road has a dashed white center line dividing two lanes.
// street lights line the road. a sun orbits the world creating
// a day/night cycle. trees and humans populate the open ground.
//
// i will:
//   1) set up the window and opengl context
//   2) load and compile shaders (with lighting + texture uniforms)
//   3) create 3d geometry (unit cube with normals and uv coords)
//   4) generate procedural textures for buildings
//   5) define world state (track, car, ai cars, buildings, lights, sun, etc)
//   6) set up view and projection matrices (9 camera modes)
//   7) write update logic for all moving parts
//   8) write draw functions for each world object
//   9) add keyboard callbacks for car control and camera switching
//   10) main loop

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "file_utils.h"
#include "math_utils.h"
#include "textures.h"

using namespace std;

// section 1: global constants and configuration
// all the tweakable numbers live here at the top of the file.
// the assignment says to "avoid hard coding values" and "define
// constants/values in the global section so that changing and testing
// the code is easier". if during the demo i am asked to change a
// value, i can find it quickly in this section.
//
// i organize them by what part of the world they control.

char g_program_title[] = "3D Car World";

// arena dimensions
// the entire scene sits inside a rectangular boundary.
// i think of it as a flat plane from (0,0) to (ARENA_W, ARENA_D) in xz,
// with y being the up direction.
const float ARENA_WIDTH = 40.0f; // x extent of the arena
const float ARENA_DEPTH = 40.0f; // z extent of the arena
const float WALL_HEIGHT = 3.0f;  // height of boundary walls
const float WALL_THICK = 0.3f;   // thickness of boundary walls

// track parameters
// the track is a distorted oval (wobbly curve) in the xz plane.
// i take the basic ellipse and add sinusoidal perturbations so
// the shape has bumps and dips, making it clearly not circular.
//
// base ellipse: x(t) = cx + A*cos(t), z(t) = cz + B*sin(t)
// distorted:    x(t) = cx + (A + p1*sin(2t) + p2*cos(3t)) * cos(t)
//               z(t) = cz + (B + p3*sin(3t) + p4*cos(2t)) * sin(t)
//
// the perturbation terms (p1, p2, p3, p4) push the curve inward
// and outward at different angles, creating a wobbly closed loop.
const float TRACK_CX = 20.0f;   // center x of the track
const float TRACK_CZ = 20.0f;   // center z of the track
const float TRACK_A = 14.0f;    // semi major axis (x direction)
const float TRACK_B = 11.0f;    // semi minor axis (z direction)
const float WOBBLE_P1 = 2.5f;   // perturbation amplitude 1
const float WOBBLE_P2 = 1.8f;   // perturbation amplitude 2
const float WOBBLE_P3 = 2.0f;   // perturbation amplitude 3
const float WOBBLE_P4 = 1.5f;   // perturbation amplitude 4
const float ROAD_WIDTH = 4.0f;  // width of the road surface
const float ROAD_Y = 0.01f;     // slightly above ground to avoid z fighting
const int TRACK_SEGMENTS = 400; // many segments = smooth curve, 3x overlap eliminates gaps

// car parameters
// the car is a collection of boxes (body, roof, wheels).
// it starts stationary at a point on the track.
// it moves forward along its heading direction.
// 'f' increases speed, 's' decreases speed.
// 'l' steers left (increases heading angle), 'r' steers right.
const float CAR_LENGTH = 2.0f;      // length of car body (along its forward axis)
const float CAR_WIDTH = 1.0f;       // width of car body
const float CAR_HEIGHT = 0.5f;      // height of car body
const float CAR_ROOF_H = 0.35f;     // height of the roof box on top
const float CAR_ROOF_L = 1.0f;      // length of roof (shorter than body)
const float CAR_WHEEL_R = 0.2f;     // wheel radius (drawn as small cubes)
const float CAR_ACCEL = 0.5f;       // speed increment per 'f' press
const float CAR_DECEL = 0.5f;       // speed decrement per 's' press
const float CAR_MAX_SPEED = 8.0f;   // maximum forward speed
const float CAR_STEER_ANGLE = 3.0f; // degrees per 'l' or 'r' press

// building parameters
// 5 buildings placed on the sides of the track.
// each building is a rectangular box with 1, 2, or 3 stories.
// each story count gets different proportions:
//   1 story: wide and shallow (shop/garage)
//   2 story: medium square (house)
//   3 story: tall and narrow (apartment)
const int NUM_BUILDINGS = 5;
const float STORY_HEIGHT = 2.0f; // height of one story

// fan/windmill parameters
// each building has a windmill on top with a nacelle box
// and 4 white blades. they spin slowly.
const int FAN_BLADES = 3;      // number of blades per turbine
const float FAN_RADIUS = 0.8f; // length of each blade
const float FAN_THICK = 0.06f; // blade thickness
float g_fan_rpm = 8.0f;        // fan rotation speed (slower, more realistic)

// swiveling gimbal light parameters
// each building has a spotlight on a pole at the front edge
// of its roof, elevated above the surface. it swings left and
// right slowly.
const float LIGHT_SWING_MAX = 30.0f;  // max swing angle in degrees
const float LIGHT_SWING_SPEED = 0.5f; // speed of swivel (slower now)

// street light parameters
// street lights are placed along the road at regular intervals.
// each has a thin pole and a warm white light on top.
const int NUM_STREET_LIGHTS = 8;
const float STREET_LIGHT_HEIGHT = 3.0f;
const float STREET_LIGHT_POLE_THICK = 0.12f;

// celestial arc parameters
// the sun and moon travel in an arch path inside the world,
// from one wall to the opposite wall. when the sun reaches
// the far wall it disappears and a moon spawns, and vice versa.
// progress goes from 0 to 1 (0 = at start wall, 1 = at far wall).
// the height follows a sine arch: y = max_height * sin(PI * progress)
// to control speed, change CELESTIAL_SPEED (lower = slower)
const float CELESTIAL_SPEED = 0.02f; // progress per second (lower = slower)
const float CELESTIAL_MAX_Y =
    20.0f;                              // peak height of the arc (below wall height limit)
const float CELESTIAL_START_X = -30.0f; // start well beyond the left world face
const float CELESTIAL_END_X =
    ARENA_WIDTH + 30.0f; // end well beyond the right face

// tree parameters
const int NUM_TREES = 7;
const float TREE_TRUNK_HEIGHT = 2.0f;
const float TREE_TRUNK_THICK = 0.3f;

// human parameters
// groups of people standing together in open ground
const int NUM_HUMANS = 8;

// ai car parameters
const int NUM_AI_CARS = 2;
const float AI_CAR_SPEED_1 =
    0.3f;                           // parameter-space speed (radians/sec, lower = slower)
const float AI_CAR_SPEED_2 = 0.25f; // second car goes a bit slower

// section 2: global variables for window and opengl objects

//
// in this section i decide:
//   what window i am drawing into
//   what opengl objects i need globally (vao, vbo, shader program, textures)
//
// question i answer here:
//   "what do i need access to from almost anywhere in my code?"

GLFWwindow *g_window = nullptr;

// vao/vbo for unit cube with normals AND uv coordinates.
// the vertex layout is [px, py, pz, nx, ny, nz, u, v] (8 floats).
// this single vao is used for everything: cars, buildings, road,
// walls, trees, humans. the shader decides whether to use the
// texture or a solid color based on the uUseTexture uniform.
GLuint g_vao_cube = 0;
GLuint g_vbo_cube = 0;

// shader program and uniform locations
GLuint g_shader_program = 0;
GLint g_model_location = -1;       // model matrix uniform
GLint g_view_location = -1;        // view matrix uniform
GLint g_proj_location = -1;        // projection matrix uniform
GLint g_color_location = -1;       // per object color uniform
GLint g_light_dir_location = -1;   // directional light direction
GLint g_light_color_location = -1; // directional light color
GLint g_camera_pos_location = -1;  // camera position for specular
GLint g_shininess_location = -1;   // material shininess
GLint g_num_point_lights_location = -1;
GLint g_point_light_pos_locations[16];
GLint g_point_light_color_locations[16];

// texture uniforms
GLint g_use_texture_location = -1; // toggle texture sampling on/off
GLint g_texture_location = -1;     // sampler2D texture unit

// building textures generated procedurally
GLuint g_tex_buildings[5]; // brick, wood, concrete, glass, sandstone

// world textures for ground, road, walls, cars, trees
GLuint g_tex_grass = 0;
GLuint g_tex_asphalt = 0;
GLuint g_tex_metal = 0;
GLuint g_tex_leaf = 0;
GLuint g_tex_stone = 0;

// view and projection matrices (computed once per frame)
static Matrix4f g_view_matrix;
static Matrix4f g_proj_matrix;

// section 3: world state data structures

//
// here i think of the game as pure data, with no rendering:
//   where is the car? where are the ai cars?
//   where are the buildings? what angle is each fan at?
//   what is the sun angle? which camera is active?
//
// i pretend i am running the game "in my head" using these numbers.
// later, i will:
//   update these numbers in update_game(dt)
//   visualize these numbers in render_frame()

// player car state
struct Car
{
  float x, z;        // position on the xz plane
  float heading_deg; // which direction the car faces (degrees)
  float speed;       // current speed (units per second, negative means reverse)
  bool collided;     // true if the car has hit something and is stuck

  float half_length; // half of CAR_LENGTH
  float half_width;  // half of CAR_WIDTH
};

Car g_car;

// ai car state
// ai cars follow the track automatically by incrementing their
// parameter t along the distorted oval curve.
//
// if i wanted to animate the ai cars moving in a different
// pattern later, i could add a path array and interpolate along it.
// for now they just follow the track center with a lane offset.
struct AICar
{
  float t;           // parameter along the track curve (0 to 2*pi)
  float speed;       // how fast t changes per second
  float lane_offset; // +1 or -1 (which lane: left or right of center)
  float r, g, b;     // car color
  float x, z;        // computed world position (updated each frame)
  float heading_deg; // computed heading (updated each frame)
  bool stopped;      // true if this ai car was hit by the player
};

AICar g_ai_cars[NUM_AI_CARS];

// building state
struct Building
{
  float x, z;         // center position on xz plane
  float width, depth; // footprint size (varies by story count)
  int stories;        // 1, 2, or 3
  float r, g, b;      // base color (used as tint, mostly overridden by texture)
  int texture_index;  // which procedural texture to use (0-4)

  float fan_angle_deg; // current rotation angle of the windmill

  float light_swing_deg; // current gimbal swing angle (-30 to +30)
  float light_swing_dir; // +1 or -1 (swinging right or left)
  float light_r, light_g,
      light_b; // light color (each building gets a different one)
};

Building g_buildings[NUM_BUILDINGS];

// street light state
// each street light has a fixed position along the track.
// it is a point light source (warm white).
struct StreetLight
{
  float x, z; // position on xz plane (at the track edge)
};

StreetLight g_street_lights[NUM_STREET_LIGHTS];

// tree state
struct Tree
{
  float x, z; // position
};

Tree g_trees[NUM_TREES];

// human state
// a human is a stack of cubes: legs, body, head.
// each one is static but i could animate them by accumulating time
// and using sin(time) to shift their z or x by a small amount
// (like 0.1 * sin(time * 2)) to make them sway as if talking.
// for now they just stand still.
struct Human
{
  float x, z;                      // position
  float facing_deg;                // which way they face
  float shirt_r, shirt_g, shirt_b; // shirt color for variety
};

Human g_humans[NUM_HUMANS];

// celestial state
// the sun arcs from one wall (x=0) to the opposite wall (x=ARENA_WIDTH)
// staying inside the world boundaries. when it reaches the far wall,
// it vanishes and a moon spawns going the same direction.
// g_celestial_progress tracks how far across the arc (0 to 1).
// g_celestial_is_sun is true for sun, false for moon.
float g_celestial_progress = 0.1f; // start slightly into the arc
bool g_celestial_is_sun = true;    // start with the sun
float g_celestial_spin_deg = 0.0f; // fast z-axis spin to blur cube into sphere

// camera state
// 9 cameras total:
//   0: sky view (overhead)
//   1: car view (on roof, looking forward)
//   2: ground view (near a building, pan left/right)
//   3-7: light source view from each building's gimbal
//   8: helicopter cam (behind and above the car)
int g_current_camera = 0;
const int NUM_CAMERAS = 9;
float g_ground_cam_pan_deg = 0.0f;

// store camera eye position for specular lighting calculations
Vector3f g_camera_eye;

// section 4: shader setup

//
// so to start with what exactly is a shader program. i remind myself:
//   vertex shader: for each vertex, decide where it appears on screen
//   fragment shader: for each pixel covered by triangles, decide its color
// a shader program is these two compiled and linked together,
// and i tell opengl to use it with glUseProgram(g_shader_program).
//
// in this project the vertex shader transforms positions through the
// model view projection pipeline, passes normals and uv coords.
// the fragment shader computes lighting and optionally samples
// a texture for the surface color.

static void create_shader_program()
{
  std::string vs_src;
  std::string fs_src;

  if (!ReadFile("shaders/shader.vs", vs_src) || !ReadFile("shaders/shader.fs", fs_src))
  {
    std::cerr << "failed to read shader source files\n";
    std::exit(1);
  }

  GLuint vs_shader = glCreateShader(GL_VERTEX_SHADER);
  if (vs_shader == 0)
  {
    std::cerr << "failed to create vertex shader object\n";
    std::exit(1);
  }

  const char *vs_src_cstr = vs_src.c_str();
  GLint vs_length = static_cast<GLint>(vs_src.size());
  glShaderSource(vs_shader, 1, &vs_src_cstr, &vs_length);
  glCompileShader(vs_shader);

  GLint status = 0;
  glGetShaderiv(vs_shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE)
  {
    char info_log[1024];
    glGetShaderInfoLog(vs_shader, 1024, nullptr, info_log);
    std::cerr << "vertex shader compile error:\n"
              << info_log << "\n";
    std::exit(1);
  }

  GLuint fs_shader = glCreateShader(GL_FRAGMENT_SHADER);
  if (fs_shader == 0)
  {
    std::cerr << "failed to create fragment shader object\n";
    std::exit(1);
  }

  const char *fs_src_cstr = fs_src.c_str();
  GLint fs_length = static_cast<GLint>(fs_src.size());
  glShaderSource(fs_shader, 1, &fs_src_cstr, &fs_length);
  glCompileShader(fs_shader);

  glGetShaderiv(fs_shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE)
  {
    char info_log[1024];
    glGetShaderInfoLog(fs_shader, 1024, nullptr, info_log);
    std::cerr << "fragment shader compile error:\n"
              << info_log << "\n";
    std::exit(1);
  }

  g_shader_program = glCreateProgram();
  if (g_shader_program == 0)
  {
    std::cerr << "failed to create shader program\n";
    std::exit(1);
  }

  glAttachShader(g_shader_program, vs_shader);
  glAttachShader(g_shader_program, fs_shader);
  glLinkProgram(g_shader_program);

  glGetProgramiv(g_shader_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE)
  {
    char info_log[1024];
    glGetProgramInfoLog(g_shader_program, 1024, nullptr, info_log);
    std::cerr << "shader program link error:\n"
              << info_log << "\n";
    std::exit(1);
  }

  glDetachShader(g_shader_program, vs_shader);
  glDetachShader(g_shader_program, fs_shader);
  glDeleteShader(vs_shader);
  glDeleteShader(fs_shader);

  glUseProgram(g_shader_program);

  // look up all uniform locations
  g_model_location = glGetUniformLocation(g_shader_program, "gModel");
  g_view_location = glGetUniformLocation(g_shader_program, "gView");
  g_proj_location = glGetUniformLocation(g_shader_program, "gProj");
  g_color_location = glGetUniformLocation(g_shader_program, "uColor");

  g_light_dir_location = glGetUniformLocation(g_shader_program, "uLightDir");
  g_light_color_location =
      glGetUniformLocation(g_shader_program, "uLightColor");
  g_camera_pos_location = glGetUniformLocation(g_shader_program, "uCameraPos");
  g_shininess_location = glGetUniformLocation(g_shader_program, "uShininess");
  g_num_point_lights_location =
      glGetUniformLocation(g_shader_program, "uNumPointLights");

  // point light uniform locations are arrays, i look up each element
  for (int i = 0; i < 16; i++)
  {
    char buf[64];
    snprintf(buf, sizeof(buf), "uPointLightPos[%d]", i);
    g_point_light_pos_locations[i] =
        glGetUniformLocation(g_shader_program, buf);
    snprintf(buf, sizeof(buf), "uPointLightColor[%d]", i);
    g_point_light_color_locations[i] =
        glGetUniformLocation(g_shader_program, buf);
  }

  // texture uniforms
  g_use_texture_location =
      glGetUniformLocation(g_shader_program, "uUseTexture");
  g_texture_location = glGetUniformLocation(g_shader_program, "uTexture");

  // set texture sampler to use texture unit 0
  glUniform1i(g_texture_location, 0);
}

// section 5: geometry creation

//
// a unit cube centered at origin goes from (-0.5, -0.5, -0.5) to
// (0.5, 0.5, 0.5). it's the single most versatile shape because
// with model matrices i can stretch it into anything.
//
// this cube has normals AND uv coordinates.
// vertex layout: [px, py, pz, nx, ny, nz, u, v] = 8 floats per vertex
//
// the uv coordinates map each face from (0,0) at one corner to
// (1,1) at the opposite corner. when a texture is bound, it covers
// the entire face once. for non-textured objects this uv data is
// present but ignored by the shader.
//
// a cube has 6 faces * 2 triangles * 3 vertices = 36 vertices
// 36 vertices * 8 floats = 288 floats total

static void create_geometry()
{
  // each row: px py pz  nx ny nz  u v
  GLfloat cube_vertices[] = {
      // front face (+z direction), normal = (0, 0, 1)
      -0.5f,
      -0.5f,
      0.5f,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      0.5f,
      -0.5f,
      0.5f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      0.0f,
      0.5f,
      0.5f,
      0.5f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      1.0f,
      -0.5f,
      -0.5f,
      0.5f,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      0.5f,
      0.5f,
      0.5f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      1.0f,
      -0.5f,
      0.5f,
      0.5f,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      1.0f,

      // back face (-z direction), normal = (0, 0, -1)
      0.5f,
      -0.5f,
      -0.5f,
      0.0f,
      0.0f,
      -1.0f,
      0.0f,
      0.0f,
      -0.5f,
      -0.5f,
      -0.5f,
      0.0f,
      0.0f,
      -1.0f,
      1.0f,
      0.0f,
      -0.5f,
      0.5f,
      -0.5f,
      0.0f,
      0.0f,
      -1.0f,
      1.0f,
      1.0f,
      0.5f,
      -0.5f,
      -0.5f,
      0.0f,
      0.0f,
      -1.0f,
      0.0f,
      0.0f,
      -0.5f,
      0.5f,
      -0.5f,
      0.0f,
      0.0f,
      -1.0f,
      1.0f,
      1.0f,
      0.5f,
      0.5f,
      -0.5f,
      0.0f,
      0.0f,
      -1.0f,
      0.0f,
      1.0f,

      // left face (-x direction), normal = (-1, 0, 0)
      -0.5f,
      -0.5f,
      -0.5f,
      -1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      -0.5f,
      -0.5f,
      0.5f,
      -1.0f,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      -0.5f,
      0.5f,
      0.5f,
      -1.0f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      -0.5f,
      -0.5f,
      -0.5f,
      -1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      -0.5f,
      0.5f,
      0.5f,
      -1.0f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      -0.5f,
      0.5f,
      -0.5f,
      -1.0f,
      0.0f,
      0.0f,
      0.0f,
      1.0f,

      // right face (+x direction), normal = (1, 0, 0)
      0.5f,
      -0.5f,
      0.5f,
      1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      0.5f,
      -0.5f,
      -0.5f,
      1.0f,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      0.5f,
      0.5f,
      -0.5f,
      1.0f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      0.5f,
      -0.5f,
      0.5f,
      1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      0.5f,
      0.5f,
      -0.5f,
      1.0f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      0.5f,
      0.5f,
      0.5f,
      1.0f,
      0.0f,
      0.0f,
      0.0f,
      1.0f,

      // top face (+y direction), normal = (0, 1, 0)
      -0.5f,
      0.5f,
      0.5f,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.5f,
      0.5f,
      0.5f,
      0.0f,
      1.0f,
      0.0f,
      1.0f,
      0.0f,
      0.5f,
      0.5f,
      -0.5f,
      0.0f,
      1.0f,
      0.0f,
      1.0f,
      1.0f,
      -0.5f,
      0.5f,
      0.5f,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.5f,
      0.5f,
      -0.5f,
      0.0f,
      1.0f,
      0.0f,
      1.0f,
      1.0f,
      -0.5f,
      0.5f,
      -0.5f,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      1.0f,

      // bottom face (-y direction), normal = (0, -1, 0)
      -0.5f,
      -0.5f,
      -0.5f,
      0.0f,
      -1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.5f,
      -0.5f,
      -0.5f,
      0.0f,
      -1.0f,
      0.0f,
      1.0f,
      0.0f,
      0.5f,
      -0.5f,
      0.5f,
      0.0f,
      -1.0f,
      0.0f,
      1.0f,
      1.0f,
      -0.5f,
      -0.5f,
      -0.5f,
      0.0f,
      -1.0f,
      0.0f,
      0.0f,
      0.0f,
      0.5f,
      -0.5f,
      0.5f,
      0.0f,
      -1.0f,
      0.0f,
      1.0f,
      1.0f,
      -0.5f,
      -0.5f,
      0.5f,
      0.0f,
      -1.0f,
      0.0f,
      0.0f,
      1.0f,
  };

  glGenVertexArrays(1, &g_vao_cube);
  glBindVertexArray(g_vao_cube);
  glGenBuffers(1, &g_vbo_cube);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo_cube);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices,
               GL_STATIC_DRAW);

  // stride is now 8 floats (3 pos + 3 normal + 2 uv)
  GLsizei stride = 8 * sizeof(GLfloat);

  // position attribute at location 0
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);

  // normal attribute at location 1
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(GLfloat)));

  // uv attribute at location 2
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(6 * sizeof(GLfloat)));

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// section 6: procedural texture loading

//
// here i generate all the building textures at startup.
// each call creates a 64x64 pixel pattern and uploads it to the gpu.
// the returned GLuint is the texture handle i bind before drawing
// each building.

static void load_textures()
{
  g_tex_buildings[0] = generate_brick_texture();
  g_tex_buildings[1] = generate_wood_texture();
  g_tex_buildings[2] = generate_concrete_texture();
  g_tex_buildings[3] = generate_glass_texture();
  g_tex_buildings[4] = generate_sandstone_texture();

  // world textures
  g_tex_grass = generate_grass_texture();
  g_tex_asphalt = generate_asphalt_texture();
  g_tex_metal = generate_metal_texture();
  g_tex_leaf = generate_leaf_texture();
  g_tex_stone = generate_stone_texture();
}

// section 7: 3d math helpers

//
// math_utils.h already provides:
//   InitTranslationTransform(x, y, z), InitScaleTransform(sx, sy, sz),
//   InitRotateTransform(rx, ry, rz), InitCameraTransform(target, up),
//   InitPersProjTransform(PersProjInfo)
//
// i wrap them in convenience functions so my draw code reads cleanly.

static Matrix4f make_translate(float tx, float ty, float tz)
{
  Matrix4f t;
  t.InitIdentity();
  t.m[0][3] = tx;
  t.m[1][3] = ty;
  t.m[2][3] = tz;
  return t;
}

static Matrix4f make_srt_3d(float sx, float sy, float sz, float rot_x_deg,
                            float rot_y_deg, float rot_z_deg, float tx,
                            float ty, float tz)
{
  Matrix4f s;
  s.InitScaleTransform(sx, sy, sz);

  Matrix4f r;
  r.InitRotateTransform(rot_x_deg, rot_y_deg, rot_z_deg);

  Matrix4f t = make_translate(tx, ty, tz);

  return t * r * s;
}

static float get_delta_time()
{
  static double last = glfwGetTime();
  double now = glfwGetTime();
  double dt = now - last;
  last = now;
  return static_cast<float>(dt);
}

// section 8: track functions (distorted oval)

//
// the track is a closed curve defined by a parametric equation.
// i start with a basic ellipse and add sinusoidal wobbles so
// the shape looks organic, not mechanical.
//
// to get the tangent direction (which way the road points at t),
// i use numerical differentiation: compute the curve at t+eps
// and t-eps, take the difference, and normalize. this avoids
// messy analytical derivatives and works perfectly.

static void track_point(float t, float &out_x, float &out_z)
{
  // the distorted radii change with the parameter t
  float a = TRACK_A + WOBBLE_P1 * sin(2.0f * t) + WOBBLE_P2 * cos(3.0f * t);
  float b = TRACK_B + WOBBLE_P3 * sin(3.0f * t) + WOBBLE_P4 * cos(2.0f * t);

  // the center of the road (not the inner or outer edge)
  // is offset inward slightly from the full radius
  float a_center = a - ROAD_WIDTH * 0.5f;
  float b_center = b - ROAD_WIDTH * 0.5f;

  out_x = TRACK_CX + a_center * cos(t);
  out_z = TRACK_CZ + b_center * sin(t);
}

static void track_tangent(float t, float &out_dx, float &out_dz)
{
  // numerical tangent: finite difference with small epsilon
  float eps = 0.001f;
  float x1, z1, x2, z2;
  track_point(t - eps, x1, z1);
  track_point(t + eps, x2, z2);

  out_dx = x2 - x1;
  out_dz = z2 - z1;

  float len = sqrt(out_dx * out_dx + out_dz * out_dz);
  if (len > 0.0001f)
  {
    out_dx /= len;
    out_dz /= len;
  }
}

// get the outward perpendicular direction at parameter t.
// the perpendicular to the tangent (dx, dz) in the xz plane
// is (-dz, dx) or (dz, -dx). i choose the one pointing away
// from the track center.
static void track_outward(float t, float &out_px, float &out_pz)
{
  float tx_d, tz_d;
  track_tangent(t, tx_d, tz_d);

  // perpendicular option 1: (-tz, tx)
  float px = -tz_d;
  float pz = tx_d;

  // check if it points away from center
  float cx, cz;
  track_point(t, cx, cz);
  float to_center_x = TRACK_CX - cx;
  float to_center_z = TRACK_CZ - cz;

  // if the dot product with to-center is positive, this perp
  // points inward, so i flip it
  if (px * to_center_x + pz * to_center_z > 0.0f)
  {
    px = -px;
    pz = -pz;
  }

  out_px = px;
  out_pz = pz;
}

// section 9: world initialization

//
// when the game starts i need to place all the objects in the world.
// buildings go at fixed spots around the track.
// the car starts at a point on the track facing along the tangent.
// ai cars start at other positions on the track.
// street lights, trees, and humans go at fixed positions.

static void init_buildings()
{
  // texture assignments: brick, wood, concrete, glass, sandstone
  int tex_indices[] = {0, 1, 2, 3, 4};

  // each building's spotlight gets a unique color
  float light_colors[][3] = {
      {1.0f, 0.3f, 0.3f}, // red
      {0.3f, 1.0f, 0.3f}, // green
      {0.3f, 0.3f, 1.0f}, // blue
      {1.0f, 1.0f, 0.3f}, // yellow
      {1.0f, 0.3f, 1.0f}, // magenta
  };

  int stories_list[] = {2, 3, 1, 2, 3};

  // hardcoded positions for each building to guarantee they never
  // overlap with the road. i picked spots in the four corners of
  // the arena and one in the center of the track loop.
  // the arena is 40x40 and the track sits roughly in the middle,
  // so the corners are always safe.
  float positions[][2] = {
      {7.0f, 7.0f},   // building 0: top-left corner (brick, 2 story)
      {34.0f, 7.0f},  // building 1: top-right corner (wood, 3 story)
      {20.0f, 20.0f}, // building 2: center of track loop (concrete, 1 story)
      {7.0f, 34.0f},  // building 3: bottom-left corner (glass, 2 story)
      {34.0f, 34.0f}, // building 4: bottom-right corner (sandstone, 3 story)
  };

  for (int i = 0; i < NUM_BUILDINGS; i++)
  {
    g_buildings[i].x = positions[i][0];
    g_buildings[i].z = positions[i][1];

    // different footprint sizes based on story count
    // 1 story: wide and shallow (shop)
    // 2 story: medium square (house)
    // 3 story: tall and narrow (apartment)
    int st = stories_list[i];
    if (st == 1)
    {
      g_buildings[i].width = 4.0f;
      g_buildings[i].depth = 2.5f;
    }
    else if (st == 2)
    {
      g_buildings[i].width = 3.0f;
      g_buildings[i].depth = 3.0f;
    }
    else
    {
      g_buildings[i].width = 2.5f;
      g_buildings[i].depth = 2.5f;
    }

    g_buildings[i].stories = st;
    g_buildings[i].r = 1.0f;
    g_buildings[i].g = 1.0f;
    g_buildings[i].b = 1.0f;
    g_buildings[i].texture_index = tex_indices[i];
    g_buildings[i].fan_angle_deg = 0.0f;
    g_buildings[i].light_swing_deg = 0.0f;
    g_buildings[i].light_swing_dir = (i % 2 == 0) ? 1.0f : -1.0f;
    g_buildings[i].light_r = light_colors[i][0];
    g_buildings[i].light_g = light_colors[i][1];
    g_buildings[i].light_b = light_colors[i][2];
  }
}

static void init_car()
{
  // park the car off the road, near building 0, as if it was parked
  // i find the building 0 position and place the car next to it,
  // facing toward the road so the player can drive onto it.
  float bld_x = g_buildings[0].x;
  float bld_z = g_buildings[0].z;

  // offset slightly from the building toward the track
  float to_road_x = TRACK_CX - bld_x;
  float to_road_z = TRACK_CZ - bld_z;
  float len = sqrt(to_road_x * to_road_x + to_road_z * to_road_z);
  if (len > 0.001f)
  {
    to_road_x /= len;
    to_road_z /= len;
  }

  g_car.x = bld_x + to_road_x * (g_buildings[0].width * 0.5f + 1.5f);
  g_car.z = bld_z + to_road_z * (g_buildings[0].depth * 0.5f + 1.5f);
  g_car.heading_deg = atan2(to_road_z, to_road_x) * 180.0f / M_PI;

  g_car.speed = 0.0f;
  g_car.collided = false;
  g_car.half_length = CAR_LENGTH * 0.5f;
  g_car.half_width = CAR_WIDTH * 0.5f;
}

static void init_ai_cars()
{
  // car 1: red, goes forward (positive speed), right lane
  g_ai_cars[0].t = M_PI * 0.5f; // starts at quarter way around
  g_ai_cars[0].speed = AI_CAR_SPEED_1;
  g_ai_cars[0].lane_offset = 1.0f; // right lane
  g_ai_cars[0].r = 0.15f; // blue
  g_ai_cars[0].g = 0.35f;
  g_ai_cars[0].b = 0.75f;
  g_ai_cars[0].stopped = false;

  // car 2: going backward (negative speed), left lane.
  // i decided to make this one bright pink so it stands out easily!
  g_ai_cars[1].t = M_PI * 1.5f; // starts at three quarter
  g_ai_cars[1].speed = -AI_CAR_SPEED_2;
  g_ai_cars[1].lane_offset = -1.0f; // left lane
  g_ai_cars[1].r = 0.95f; // pink color
  g_ai_cars[1].g = 0.3f;
  g_ai_cars[1].b = 0.7f;
  g_ai_cars[1].stopped = false;
}

static void init_street_lights()
{
  // place street lights at evenly spaced parameter values around the track
  // each one is offset slightly to the outside of the track
  for (int i = 0; i < NUM_STREET_LIGHTS; i++)
  {
    float t = i * (2.0f * M_PI / NUM_STREET_LIGHTS);
    float cx, cz;
    track_point(t, cx, cz);

    float out_x, out_z;
    track_outward(t, out_x, out_z);

    // place just outside the road edge
    g_street_lights[i].x = cx + out_x * (ROAD_WIDTH * 0.5f + 0.5f);
    g_street_lights[i].z = cz + out_z * (ROAD_WIDTH * 0.5f + 0.5f);
  }
}

static void init_trees()
{
  // fixed positions for trees, scattered around the arena
  // avoiding the track and buildings
  float tree_positions[][2] = {
      {4.0f, 4.0f},
      {36.0f, 5.0f},
      {5.0f, 35.0f},
      {35.0f, 36.0f},
      {3.0f, 20.0f},
      {37.0f, 20.0f},
      {20.0f, 3.0f},
  };

  for (int i = 0; i < NUM_TREES; i++)
  {
    g_trees[i].x = tree_positions[i][0];
    g_trees[i].z = tree_positions[i][1];
  }
}

static void init_humans()
{
  // 8 people spread around the arena in pairs.
  // buildings are at (7,7), (34,7), (20,20), (7,34), (34,34)
  // so i avoid those spots and place people in the open areas.

  // pair 1: near the top edge, between the two top buildings
  g_humans[0].x = 20.0f;
  g_humans[0].z = 5.0f;
  g_humans[0].facing_deg = 90.0f;
  g_humans[0].shirt_r = 0.8f;
  g_humans[0].shirt_g = 0.2f;
  g_humans[0].shirt_b = 0.2f;

  g_humans[1].x = 21.0f;
  g_humans[1].z = 5.5f;
  g_humans[1].facing_deg = -90.0f;
  g_humans[1].shirt_r = 0.2f;
  g_humans[1].shirt_g = 0.4f;
  g_humans[1].shirt_b = 0.8f;

  // pair 2: near the bottom edge, between the two bottom buildings
  g_humans[2].x = 20.0f;
  g_humans[2].z = 36.0f;
  g_humans[2].facing_deg = -45.0f;
  g_humans[2].shirt_r = 0.9f;
  g_humans[2].shirt_g = 0.7f;
  g_humans[2].shirt_b = 0.1f;

  g_humans[3].x = 21.0f;
  g_humans[3].z = 36.5f;
  g_humans[3].facing_deg = 135.0f;
  g_humans[3].shirt_r = 0.3f;
  g_humans[3].shirt_g = 0.7f;
  g_humans[3].shirt_b = 0.3f;

  // pair 3: left side of the arena, between the two left buildings
  g_humans[4].x = 4.0f;
  g_humans[4].z = 20.0f;
  g_humans[4].facing_deg = 0.0f;
  g_humans[4].shirt_r = 0.6f;
  g_humans[4].shirt_g = 0.2f;
  g_humans[4].shirt_b = 0.7f;

  g_humans[5].x = 5.0f;
  g_humans[5].z = 20.5f;
  g_humans[5].facing_deg = 180.0f;
  g_humans[5].shirt_r = 0.2f;
  g_humans[5].shirt_g = 0.8f;
  g_humans[5].shirt_b = 0.6f;

  // pair 4: right side of the arena, between the two right buildings
  g_humans[6].x = 37.0f;
  g_humans[6].z = 20.0f;
  g_humans[6].facing_deg = 45.0f;
  g_humans[6].shirt_r = 0.9f;
  g_humans[6].shirt_g = 0.4f;
  g_humans[6].shirt_b = 0.1f;

  g_humans[7].x = 37.5f;
  g_humans[7].z = 21.0f;
  g_humans[7].facing_deg = -135.0f;
  g_humans[7].shirt_r = 0.1f;
  g_humans[7].shirt_g = 0.5f;
  g_humans[7].shirt_b = 0.9f;
}

static void init_world()
{
  // setting up the initial state of the world here.
  // i want to make sure the game starts with everything in a clean state
  // before the first frame is rendered, so everything is placed nicely.
  init_buildings();
  init_car();
  init_ai_cars();
  init_street_lights();
  init_trees();
  init_humans();
  g_current_camera = 0;
  g_ground_cam_pan_deg = 0.0f;
  g_celestial_progress = 0.1f;
  g_celestial_is_sun = true;
}

// section 10: update logic

//
// this is the "brain" of the program. no rendering happens here,
// only numbers are updated each frame.

static bool check_car_building_collision(const Car &car, const Building &bld)
{
  // proper axis-aligned bounding box (aabb) collision.
  // instead of using a single circle radius (which is too generous),
  // i check overlap on each axis independently using the actual
  // half_length and half_width of the car and building.
  // this gives a tight rectangular hit box.
  float bld_half_w = bld.width * 0.5f;
  float bld_half_d = bld.depth * 0.5f;

  bool overlap_x = std::abs(car.x - bld.x) < (car.half_length + bld_half_w);
  bool overlap_z = std::abs(car.z - bld.z) < (car.half_width + bld_half_d);

  return overlap_x && overlap_z;
}

static bool check_car_wall_collision(const Car &car)
{
  // check each wall with the car's actual size, not a bloated circle
  float hl = car.half_length;
  float hw = car.half_width;
  float r =
      std::max(hl, hw); // still use max for walls since car can be rotated
  if (car.x - r < 0.0f || car.x + r > ARENA_WIDTH)
    return true;
  if (car.z - r < 0.0f || car.z + r > ARENA_DEPTH)
    return true;
  return false;
}

static bool check_car_ai_collision(const Car &car, const AICar &ai)
{
  // tight aabb between player car and ai car
  float ai_half_l = CAR_LENGTH * 0.5f;
  float ai_half_w = CAR_WIDTH * 0.5f;
  float dist_x = std::abs(car.x - ai.x);
  float dist_z = std::abs(car.z - ai.z);
  return (dist_x < car.half_length + ai_half_l) &&
         (dist_z < car.half_width + ai_half_w);
}

// check if the car overlaps with a tree trunk
static bool check_car_tree_collision(const Car &car, const Tree &tree)
{
  float trunk_half = TREE_TRUNK_THICK * 0.5f;
  float dist_x = std::abs(car.x - tree.x);
  float dist_z = std::abs(car.z - tree.z);
  return (dist_x < car.half_length + trunk_half) &&
         (dist_z < car.half_width + trunk_half);
}

// check if the car overlaps with a standing human
static bool check_car_human_collision(const Car &car, const Human &human)
{
  float human_half = 0.15f; // a human is roughly 0.3 units wide
  float dist_x = std::abs(car.x - human.x);
  float dist_z = std::abs(car.z - human.z);
  return (dist_x < car.half_length + human_half) &&
         (dist_z < car.half_width + human_half);
}

static void update_game(float dt)
{
  // car movement
  // once the car has collided, ALL movement is blocked.
  // the player cannot accelerate, steer, or do anything until reset.
  if (!g_car.collided)
  {
    float heading_rad = g_car.heading_deg * M_PI / 180.0f;
    float dx = g_car.speed * cos(heading_rad) * dt;
    float dz = g_car.speed * sin(heading_rad) * dt;

    float old_x = g_car.x;
    float old_z = g_car.z;

    g_car.x += dx;
    g_car.z += dz;

    // check wall collision
    if (check_car_wall_collision(g_car))
    {
      g_car.x = old_x;
      g_car.z = old_z;
      g_car.speed = 0.0f;
      g_car.collided = true;
      std::cout << "car hit a wall! press 'n' to reset.\n";
    }

    // check building collisions
    if (!g_car.collided)
    {
      for (int i = 0; i < NUM_BUILDINGS; i++)
      {
        if (check_car_building_collision(g_car, g_buildings[i]))
        {
          g_car.x = old_x;
          g_car.z = old_z;
          g_car.speed = 0.0f;
          g_car.collided = true;
          std::cout << "car hit building " << i << "! press 'n' to reset.\n";
          break;
        }
      }
    }

    // check ai car collisions
    // basically checking if the player bumped into any of the ai cars
    if (!g_car.collided)
    {
      for (int i = 0; i < NUM_AI_CARS; i++)
      {
        if (check_car_ai_collision(g_car, g_ai_cars[i]))
        {
          g_car.x = old_x;
          g_car.z = old_z;
          g_car.speed = 0.0f;
          g_car.collided = true;
          // the hit ai car also stops moving
          g_ai_cars[i].stopped = true;
          g_ai_cars[i].speed = 0.0f;
          std::cout << "car hit ai car " << i << "! press 'n' to reset.\n";
          break;
        }
      }
    }

    // check tree collisions
    if (!g_car.collided)
    {
      for (int i = 0; i < NUM_TREES; i++)
      {
        if (check_car_tree_collision(g_car, g_trees[i]))
        {
          g_car.x = old_x;
          g_car.z = old_z;
          g_car.speed = 0.0f;
          g_car.collided = true;
          std::cout << "car hit a tree! press 'n' to reset.\n";
          break;
        }
      }
    }

    // check human collisions
    if (!g_car.collided)
    {
      for (int i = 0; i < NUM_HUMANS; i++)
      {
        if (check_car_human_collision(g_car, g_humans[i]))
        {
          g_car.x = old_x;
          g_car.z = old_z;
          g_car.speed = 0.0f;
          g_car.collided = true;
          std::cout << "car hit a person! press 'n' to reset.\n";
          break;
        }
      }
    }
  }

  // ai car movement
  // each ai car moves along the track by incrementing its t parameter.
  // if the ai car was hit by the player, it stays frozen.
  for (int i = 0; i < NUM_AI_CARS; i++)
  {
    if (!g_ai_cars[i].stopped)
    {
      g_ai_cars[i].t += g_ai_cars[i].speed * dt;
      if (g_ai_cars[i].t > 2.0f * M_PI)
        g_ai_cars[i].t -= 2.0f * M_PI;
      if (g_ai_cars[i].t < 0.0f)
        g_ai_cars[i].t += 2.0f * M_PI;
    }

    // compute world position on the track center
    float cx, cz;
    track_point(g_ai_cars[i].t, cx, cz);

    // perpendicular offset for lane
    float tx_d, tz_d;
    track_tangent(g_ai_cars[i].t, tx_d, tz_d);
    float perp_x = -tz_d;
    float perp_z = tx_d;

    g_ai_cars[i].x =
        cx + perp_x * g_ai_cars[i].lane_offset * (ROAD_WIDTH * 0.22f);
    g_ai_cars[i].z =
        cz + perp_z * g_ai_cars[i].lane_offset * (ROAD_WIDTH * 0.22f);

    // heading from tangent direction
    float heading = atan2(tz_d, tx_d) * 180.0f / M_PI;
    if (g_ai_cars[i].speed < 0.0f)
      heading += 180.0f;
    g_ai_cars[i].heading_deg = heading;
  } // end for ai cars

  // after ai cars move, check if any moving ai car hit the stopped player
  // this handles the case where player hits ai car 1, stays stuck,
  // and ai car 2 comes along and runs into the stationary player.
  if (g_car.collided)
  {
    for (int i = 0; i < NUM_AI_CARS; i++)
    {
      if (!g_ai_cars[i].stopped &&
          check_car_ai_collision(g_car, g_ai_cars[i]))
      {
        g_ai_cars[i].stopped = true;
        g_ai_cars[i].speed = 0.0f;
        std::cout << "ai car " << i << " hit the stopped player!\n";
      }
    }
  }

  // fan/windmill rotation
  float fan_deg_per_sec = g_fan_rpm * 6.0f;
  for (int i = 0; i < NUM_BUILDINGS; i++)
  {
    g_buildings[i].fan_angle_deg += fan_deg_per_sec * dt;
    if (g_buildings[i].fan_angle_deg >= 360.0f)
      g_buildings[i].fan_angle_deg -= 360.0f;
  }

  // swiveling gimbal light animation
  for (int i = 0; i < NUM_BUILDINGS; i++)
  {
    float swing_speed_deg = LIGHT_SWING_SPEED * 180.0f / M_PI;
    g_buildings[i].light_swing_deg +=
        g_buildings[i].light_swing_dir * swing_speed_deg * dt;

    if (g_buildings[i].light_swing_deg > LIGHT_SWING_MAX)
    {
      g_buildings[i].light_swing_deg = LIGHT_SWING_MAX;
      g_buildings[i].light_swing_dir = -1.0f;
    }
    if (g_buildings[i].light_swing_deg < -LIGHT_SWING_MAX)
    {
      g_buildings[i].light_swing_deg = -LIGHT_SWING_MAX;
      g_buildings[i].light_swing_dir = 1.0f;
    }
  }

  // celestial arc movement
  // progress goes from 0 to 1 (wall to wall).
  // when it exceeds 1, the current body disappears and the
  // next one (moon or sun) spawns at the start.
  g_celestial_progress += CELESTIAL_SPEED * dt;
  if (g_celestial_progress > 1.0f)
  {
    g_celestial_progress = 0.0f;
    g_celestial_is_sun = !g_celestial_is_sun; // swap sun and moon
  }

  // spin the celestial body at high rpm around multiple axes
  // this blurs the cube edges and makes it look more like a sphere.
  // 2160 deg/sec = 6 full rotations per second for a strong blur effect
  g_celestial_spin_deg += 2160.0f * dt;
  if (g_celestial_spin_deg >= 360.0f)
    g_celestial_spin_deg -= 360.0f;
}

// section 11: camera setup

//
// how does the view matrix work?
//
// in real life, i move a camera to see things from different angles.
// in opengl, the "camera" is always at the origin looking down -z.
// so instead of moving the camera, i move the entire world in the
// opposite direction. that's what the view matrix does.
//
// i have 9 cameras:
//   0: sky view
//   1: car view
//   2: ground view with panning
//   3-7: light source views from each building's gimbal
//   8: helicopter cam

static void setup_projection_matrix()
{
  int fbw = 0, fbh = 0;
  glfwGetFramebufferSize(g_window, &fbw, &fbh);
  if (fbw <= 0 || fbh <= 0)
    return;

  PersProjInfo proj_info;
  proj_info.FOV = 60.0f;
  proj_info.Width = static_cast<float>(fbw);
  proj_info.Height = static_cast<float>(fbh);
  proj_info.zNear = 0.1f;
  proj_info.zFar = 200.0f;

  g_proj_matrix.InitPersProjTransform(proj_info);
}

static void setup_view_matrix()
{
  Vector3f eye, target_dir, up;

  switch (g_current_camera)
  {
  case 0: // sky view
    eye = Vector3f(TRACK_CX, 50.0f, TRACK_CZ);
    target_dir = Vector3f(0.0f, -1.0f, 0.0f);
    up = Vector3f(0.0f, 0.0f, -1.0f);
    break;

  case 1: // car view
  {
    float heading_rad = g_car.heading_deg * M_PI / 180.0f;
    float forward_x = cos(heading_rad);
    float forward_z = sin(heading_rad);

    float roof_y = CAR_HEIGHT + CAR_ROOF_H + 0.1f;
    eye = Vector3f(g_car.x + forward_x * (CAR_LENGTH * 0.3f), roof_y,
                   g_car.z + forward_z * (CAR_LENGTH * 0.3f));
    target_dir = Vector3f(forward_x, -0.1f, forward_z);
    target_dir.Normalize();
    up = Vector3f(0.0f, 1.0f, 0.0f);
    break;
  }

  case 2: // ground view
  {
    // the ground camera stands near building 0, elevated to eye height,
    // looking toward the road. i push it far enough from the building
    // so the building doesn't block the view.
    float bld_x = g_buildings[0].x;
    float bld_z = g_buildings[0].z;
    float cam_height = 2.5f;

    float to_road_x = TRACK_CX - bld_x;
    float to_road_z = TRACK_CZ - bld_z;
    float len = sqrt(to_road_x * to_road_x + to_road_z * to_road_z);
    if (len > 0.001f)
    {
      to_road_x /= len;
      to_road_z /= len;
    }

    // push 6 units from the building center so we clear the building body
    eye = Vector3f(bld_x + to_road_x * 6.0f, cam_height,
                   bld_z + to_road_z * 6.0f);

    float pan_rad = g_ground_cam_pan_deg * M_PI / 180.0f;
    float panned_x = to_road_x * cos(pan_rad) - to_road_z * sin(pan_rad);
    float panned_z = to_road_x * sin(pan_rad) + to_road_z * cos(pan_rad);

    target_dir = Vector3f(panned_x, -0.1f, panned_z);
    target_dir.Normalize();
    up = Vector3f(0.0f, 1.0f, 0.0f);
    break;
  }

  case 3:
  case 4:
  case 5:
  case 6:
  case 7: // light source views (one per building)
  {
    // cameras 3 through 7 correspond to buildings 0 through 4.
    // the camera is placed well in front of the building looking
    // at the road, so the player can see the scene from the
    // light's perspective without the building blocking the view.
    //
    // special case: building 2 sits at the track center (20,20),
    // so to_road direction would be (0,0). for that building i
    // use a fixed fallback direction instead.
    int bi = g_current_camera - 3;
    float bld_x = g_buildings[bi].x;
    float bld_z = g_buildings[bi].z;
    float bld_top_y = g_buildings[bi].stories * STORY_HEIGHT;

    float to_road_x = TRACK_CX - bld_x;
    float to_road_z = TRACK_CZ - bld_z;
    float len = sqrt(to_road_x * to_road_x + to_road_z * to_road_z);
    if (len > 0.001f)
    {
      to_road_x /= len;
      to_road_z /= len;
    }
    else
    {
      // building is at the track center — no meaningful "toward road"
      // direction. use a fixed direction so the camera isnt broken.
      to_road_x = 1.0f;
      to_road_z = 0.0f;
    }

    // push the camera further out from the building so it can see the road
    float cam_offset = g_buildings[bi].width * 0.5f + 3.0f;
    float light_x = bld_x + to_road_x * cam_offset;
    float light_z = bld_z + to_road_z * cam_offset;
    float light_y = bld_top_y + 1.5f;

    eye = Vector3f(light_x, light_y, light_z);

    // look direction follows the swing angle, pointing toward the road
    float swing_rad = g_buildings[bi].light_swing_deg * M_PI / 180.0f;
    float swung_x = to_road_x * cos(swing_rad) - to_road_z * sin(swing_rad);
    float swung_z = to_road_x * sin(swing_rad) + to_road_z * cos(swing_rad);

    target_dir = Vector3f(swung_x, -0.3f, swung_z);
    target_dir.Normalize();
    up = Vector3f(0.0f, 1.0f, 0.0f);
    break;
  }

  case 8: // helicopter cam
  {
    float heading_rad = g_car.heading_deg * M_PI / 180.0f;
    float forward_x = cos(heading_rad);
    float forward_z = sin(heading_rad);

    float behind_dist = 8.0f;
    float above_dist = 5.0f;
    eye = Vector3f(g_car.x - forward_x * behind_dist, above_dist,
                   g_car.z - forward_z * behind_dist);

    target_dir = Vector3f(g_car.x - eye.x, -above_dist * 0.3f, g_car.z - eye.z);
    target_dir.Normalize();
    up = Vector3f(0.0f, 1.0f, 0.0f);
    break;
  }

  default:
    eye = Vector3f(TRACK_CX, 30.0f, TRACK_CZ + 30.0f);
    target_dir = Vector3f(0.0f, -0.5f, -1.0f);
    target_dir.Normalize();
    up = Vector3f(0.0f, 1.0f, 0.0f);
    break;
  }

  g_camera_eye = eye;

  Matrix4f cam_translate = make_translate(-eye.x, -eye.y, -eye.z);
  Matrix4f cam_rotate;
  cam_rotate.InitCameraTransform(target_dir, up);
  g_view_matrix = cam_rotate * cam_translate;
}

// section 12: draw functions

//
// each draw function:
//   1. sets the object color or binds a texture
//   2. sets the material shininess
//   3. builds a model matrix to position, scale, and rotate the cube
//   4. uploads the model matrix to the gpu
//   5. draws 36 vertices of the unit cube

static void set_object_color(float r, float g, float b, float a = 1.0f)
{
  glUniform4f(g_color_location, r, g, b, a);
}

static void set_shininess(float s) { glUniform1f(g_shininess_location, s); }

static void enable_texture(GLuint tex_id)
{
  glUniform1i(g_use_texture_location, 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_id);
}

static void disable_texture() { glUniform1i(g_use_texture_location, 0); }

static void upload_model_matrix(const Matrix4f &model)
{
  glUniformMatrix4fv(g_model_location, 1, GL_TRUE, &model.m[0][0]);
}

static void upload_view_proj()
{
  glUniformMatrix4fv(g_view_location, 1, GL_TRUE, &g_view_matrix.m[0][0]);
  glUniformMatrix4fv(g_proj_location, 1, GL_TRUE, &g_proj_matrix.m[0][0]);
}

static void upload_lighting()
{
  // celestial arc based lighting
  // the sun's position along the arc determines the light direction.
  // day_factor is based on the arc height (peaks at the middle).
  // when its the moon's turn, lighting is very dim.
  float arc_height = sin(M_PI * g_celestial_progress); // 0 at edges, 1 at peak
  float day_factor = g_celestial_is_sun ? arc_height : 0.0f;

  // compute sun direction from the celestial body's position
  // toward the center of the world
  float cel_x = CELESTIAL_START_X +
                g_celestial_progress * (CELESTIAL_END_X - CELESTIAL_START_X);
  float cel_y = CELESTIAL_MAX_Y * arc_height;
  float cel_z = ARENA_DEPTH * 0.5f;

  // direction from center of world toward the celestial body
  float dir_x = cel_x - TRACK_CX;
  float dir_y = cel_y;
  float dir_z = cel_z - TRACK_CZ;
  float dir_len = sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
  if (dir_len > 0.001f)
  {
    dir_x /= dir_len;
    dir_y /= dir_len;
    dir_z /= dir_len;
  }

  glUniform3f(g_light_dir_location, dir_x, dir_y, dir_z);

  // sun light color: warm white scaled by day factor
  // moon provides a very faint bluish ambient only
  float moon_factor = g_celestial_is_sun ? 0.0f : arc_height * 0.15f;
  glUniform3f(g_light_color_location, 1.0f * day_factor + 0.05f * moon_factor,
              0.95f * day_factor + 0.08f * moon_factor,
              0.85f * day_factor + 0.15f * moon_factor);

  // camera position for specular
  glUniform3f(g_camera_pos_location, g_camera_eye.x, g_camera_eye.y,
              g_camera_eye.z);

  // point lights: first the 5 building gimbal lights, then the 8 street lights
  int total_lights = NUM_BUILDINGS + NUM_STREET_LIGHTS;
  glUniform1i(g_num_point_lights_location, total_lights);

  // building gimbal lights
  for (int i = 0; i < NUM_BUILDINGS; i++)
  {
    float bld_top_y = g_buildings[i].stories * STORY_HEIGHT;

    // gimbal is at the front of the building, elevated on a pole
    float to_road_x = TRACK_CX - g_buildings[i].x;
    float to_road_z = TRACK_CZ - g_buildings[i].z;
    float tr_len = sqrt(to_road_x * to_road_x + to_road_z * to_road_z);
    if (tr_len > 0.001f)
    {
      to_road_x /= tr_len;
      to_road_z /= tr_len;
    }
    else
    {
      to_road_x = 1.0f;
      to_road_z = 0.0f;
    }

    float light_x =
        g_buildings[i].x + to_road_x * (g_buildings[i].width * 0.4f);
    float light_z =
        g_buildings[i].z + to_road_z * (g_buildings[i].depth * 0.4f);
    float light_y = bld_top_y + 0.7f;

    glUniform3f(g_point_light_pos_locations[i], light_x, light_y, light_z);
    glUniform3f(g_point_light_color_locations[i], g_buildings[i].light_r,
                g_buildings[i].light_g, g_buildings[i].light_b);
  }

  // street lights: warm white, always on but brighter at night
  float street_brightness = 0.5f + 0.5f * (1.0f - day_factor);
  for (int i = 0; i < NUM_STREET_LIGHTS; i++)
  {
    int idx = NUM_BUILDINGS + i;
    glUniform3f(g_point_light_pos_locations[idx], g_street_lights[i].x,
                STREET_LIGHT_HEIGHT + 0.1f, g_street_lights[i].z);
    glUniform3f(g_point_light_color_locations[idx], 0.9f * street_brightness,
                0.8f * street_brightness, 0.5f * street_brightness);
  }
}

static void draw_ground()
{
  // draw the big green grassy field that everything sits on.
  // using a huge flat cube pushed slightly down so it doesn't z-fight with the road.
  enable_texture(g_tex_grass);
  set_object_color(0.25f, 0.45f, 0.20f);
  set_shininess(0.0f);
  Matrix4f model = make_srt_3d(ARENA_WIDTH, 0.05f, ARENA_DEPTH, 0, 0, 0,
                               ARENA_WIDTH / 2.0f, -0.025f, ARENA_DEPTH / 2.0f);
  upload_model_matrix(model);
  glBindVertexArray(g_vao_cube);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
  disable_texture();
}

static void draw_track()
{
  // the road surface is drawn as many small flat boxes placed along
  // the distorted oval curve. using 200 segments with 1.3x overlap
  // ensures no green gaps show through even at tight curves.
  //
  // after drawing the road surface, i draw dashed white boxes
  // along the center line to create the two-lane marking.

  enable_texture(g_tex_asphalt);
  glBindVertexArray(g_vao_cube);

  float dt_param = 2.0f * M_PI / TRACK_SEGMENTS;

  // pass 1: dark asphalt road surface with texture
  set_object_color(0.3f, 0.3f, 0.32f);
  set_shininess(0.0f);

  for (int i = 0; i < TRACK_SEGMENTS; i++)
  {
    float t = i * dt_param;
    float t_next = (i + 1) * dt_param;

    float mid_t = (t + t_next) * 0.5f;
    float cx, cz;
    track_point(mid_t, cx, cz);

    float tx, tz;
    track_tangent(mid_t, tx, tz);

    float angle_deg = atan2(tz, tx) * 180.0f / M_PI;

    float x1, z1, x2, z2;
    track_point(t, x1, z1);
    track_point(t_next, x2, z2);
    float seg_len = sqrt((x2 - x1) * (x2 - x1) + (z2 - z1) * (z2 - z1));

    // 3x overlap ensures no green gaps even at sharp curves
    Matrix4f model = make_srt_3d(seg_len * 3.0f, 0.05f, ROAD_WIDTH, 0,
                                 angle_deg, 0, cx, ROAD_Y, cz);
    upload_model_matrix(model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  disable_texture();

  // pass 2: dashed white center line
  // every other group of 3 segments gets a white dash
  set_object_color(0.9f, 0.9f, 0.9f);

  for (int i = 0; i < TRACK_SEGMENTS; i++)
  {
    // create a dashed pattern: 3 on, 3 off
    if ((i / 3) % 2 != 0)
      continue;

    float t = (i + 0.5f) * dt_param;
    float cx, cz;
    track_point(t, cx, cz);

    float tx, tz;
    track_tangent(t, tx, tz);

    float angle_deg = atan2(tz, tx) * 180.0f / M_PI;

    // small thin white box on the center of the road
    float dash_len = dt_param * 10.0f; // short dash length
    Matrix4f model = make_srt_3d(dash_len, 0.06f, 0.12f, 0, angle_deg, 0, cx,
                                 ROAD_Y + 0.01f, cz);
    upload_model_matrix(model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  glBindVertexArray(0);
}

static void draw_walls()
{
  enable_texture(g_tex_stone);
  set_object_color(0.7f, 0.65f, 0.60f);
  set_shininess(0.0f);

  glBindVertexArray(g_vao_cube);

  // back wall at z = 0
  Matrix4f back = make_srt_3d(ARENA_WIDTH, WALL_HEIGHT, WALL_THICK, 0, 0, 0,
                              ARENA_WIDTH / 2.0f, WALL_HEIGHT / 2.0f, 0.0f);
  upload_model_matrix(back);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // front wall at z = ARENA_DEPTH
  Matrix4f front =
      make_srt_3d(ARENA_WIDTH, WALL_HEIGHT, WALL_THICK, 0, 0, 0,
                  ARENA_WIDTH / 2.0f, WALL_HEIGHT / 2.0f, ARENA_DEPTH);
  upload_model_matrix(front);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // left wall at x = 0
  Matrix4f left = make_srt_3d(WALL_THICK, WALL_HEIGHT, ARENA_DEPTH, 0, 0, 0,
                              0.0f, WALL_HEIGHT / 2.0f, ARENA_DEPTH / 2.0f);
  upload_model_matrix(left);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // right wall at x = ARENA_WIDTH
  Matrix4f right =
      make_srt_3d(WALL_THICK, WALL_HEIGHT, ARENA_DEPTH, 0, 0, 0, ARENA_WIDTH,
                  WALL_HEIGHT / 2.0f, ARENA_DEPTH / 2.0f);
  upload_model_matrix(right);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  glBindVertexArray(0);
  disable_texture();
}

static void draw_buildings()
{
  // for each building i draw:
  //   1. the main body (textured with brick/wood/etc)
  //   2. floor separator lines between stories (thin dark boxes)
  //   3. a roof edge (slightly wider slab on top)
  //   4. the windmill (tower + nacelle + blades)
  //   5. the gimbal light (elevated on a pole at the front)

  glBindVertexArray(g_vao_cube);

  for (int i = 0; i < NUM_BUILDINGS; i++)
  {
    Building &bld = g_buildings[i];
    float bld_height = bld.stories * STORY_HEIGHT;

    // direction from building toward the road (used for placing
    // fans and lights on the "front" face).
    // special case: building 2 at the track center has zero direction,
    // so we fall back to (1, 0) to avoid broken orientations.
    float to_road_x = TRACK_CX - bld.x;
    float to_road_z = TRACK_CZ - bld.z;
    float tr_len = sqrt(to_road_x * to_road_x + to_road_z * to_road_z);
    if (tr_len > 0.001f)
    {
      to_road_x /= tr_len;
      to_road_z /= tr_len;
    }
    else
    {
      to_road_x = 1.0f;
      to_road_z = 0.0f;
    }

    // 1. building body with texture
    enable_texture(g_tex_buildings[bld.texture_index]);
    set_object_color(1.0f, 1.0f,
                     1.0f); // white tint so texture shows true colors
    set_shininess(2.0f);

    Matrix4f body = make_srt_3d(bld.width, bld_height, bld.depth, 0, 0, 0,
                                bld.x, bld_height / 2.0f, bld.z);
    upload_model_matrix(body);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    disable_texture();

    // 1b. add a door on the front face (but not on the glass buildings)
    // the buildings are axis-aligned boxes (no rotation), so we can't just push
    // the door out in the exact 'to_road' direction or it might end up inside a corner!
    // i need to snap the door to the cardinal face (N, S, E, or W) that points most toward the road.
    if (bld.texture_index != 3) // 3 is the glass texture
    {
      float door_dir_x = 0.0f;
      float door_dir_z = 0.0f;
      
      if (std::abs(to_road_x) > std::abs(to_road_z)) {
        door_dir_x = (to_road_x > 0.0f) ? 1.0f : -1.0f;
      } else {
        door_dir_z = (to_road_z > 0.0f) ? 1.0f : -1.0f;
      }

      float front_x = bld.x + door_dir_x * (bld.width * 0.5f + 0.05f);
      float front_z = bld.z + door_dir_z * (bld.depth * 0.5f + 0.05f);
      float face_angle = atan2(door_dir_z, door_dir_x) * 180.0f / M_PI;

      // draw the door right at the bottom
      set_object_color(0.2f, 0.1f, 0.05f); // dark brown wood color for the door
      set_shininess(0.0f);
      Matrix4f door = make_srt_3d(0.05f, 1.5f, 0.8f, 0, face_angle, 0, 
                                  front_x, 0.75f, front_z);
      upload_model_matrix(door);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 2. floor separator lines between stories
    // thin dark horizontal boxes that protrude slightly
    if (bld.stories > 1)
    {
      set_object_color(0.2f, 0.18f, 0.15f);
      set_shininess(0.0f);
      for (int s = 1; s < bld.stories; s++)
      {
        float sep_y = s * STORY_HEIGHT;
        Matrix4f sep = make_srt_3d(bld.width + 0.1f, 0.08f, bld.depth + 0.1f, 0,
                                   0, 0, bld.x, sep_y, bld.z);
        upload_model_matrix(sep);
        glDrawArrays(GL_TRIANGLES, 0, 36);
      }
    }

    // 3. roof edge (slightly wider slab on top)
    set_object_color(0.3f, 0.28f, 0.25f);
    set_shininess(0.0f);
    {
      Matrix4f roof = make_srt_3d(bld.width + 0.2f, 0.1f, bld.depth + 0.2f, 0,
                                  0, 0, bld.x, bld_height + 0.05f, bld.z);
      upload_model_matrix(roof);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 4. windmill (proper wind turbine: tower + horizontal nacelle + blades)
    // a real wind turbine has:
    //   a vertical tower (tall thin pole rising from the roof)
    //   a horizontal nacelle (housing) on top of the tower
    //   blades attached to the front of the nacelle, spinning in a vertical
    //   plane
    float fan_facing_deg = atan2(to_road_z, to_road_x) * 180.0f / M_PI;
    float tower_height = 1.2f;
    float tower_x = bld.x; // center of roof
    float tower_z = bld.z;
    float tower_top_y = bld_height + tower_height;

    // tower (thin vertical pole)
    set_object_color(0.5f, 0.5f, 0.5f); // grey tower
    set_shininess(0.0f);
    {
      Matrix4f tower = make_srt_3d(0.15f, tower_height, 0.15f, 0, 0, 0, tower_x,
                                   bld_height + tower_height / 2.0f, tower_z);
      upload_model_matrix(tower);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // nacelle (horizontal box on top of the tower, pointing toward the road)
    set_object_color(0.15f, 0.15f, 0.15f); // dark grey nacelle
    set_shininess(0.0f);
    float nac_len = 0.6f;
    float nac_x = tower_x + to_road_x * (nac_len * 0.3f);
    float nac_z = tower_z + to_road_z * (nac_len * 0.3f);
    {
      // nacelle is horizontal: long along facing direction
      Matrix4f nac = make_srt_3d(nac_len, 0.25f, 0.25f, 0, fan_facing_deg, 0,
                                 nac_x, tower_top_y, nac_z);
      upload_model_matrix(nac);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // hub and blades placed right at the front edge of the nacelle!
    // before, this was floating away from the box. so i changed it to be exactly
    // at the front of the nacelle (nac_x + half the nacelle length).
    float hub_x = nac_x + to_road_x * (nac_len * 0.5f);
    float hub_z = nac_z + to_road_z * (nac_len * 0.5f);
    set_object_color(0.2f, 0.2f, 0.2f);
    {
      Matrix4f hub =
          make_srt_3d(0.12f, 0.12f, 0.12f, 0, 0, 0, hub_x, tower_top_y, hub_z);
      upload_model_matrix(hub);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // blades (3 white blades spinning in a vertical plane)
    // in a real wind turbine the blades extend radially outward
    // from the hub (along Y in local space) and spin around
    // the nacelle axis (X in local space). the face_rot then
    // rotates the whole assembly so X points toward the road.
    set_object_color(0.95f, 0.95f, 0.93f); // white blades
    set_shininess(0.0f);

    for (int j = 0; j < FAN_BLADES; j++)
    {
      float blade_angle = bld.fan_angle_deg + j * (360.0f / FAN_BLADES);

      // blade shape: thin in X and Z, long in Y (radial direction)
      Matrix4f s;
      s.InitScaleTransform(FAN_THICK * 1.5f, FAN_RADIUS, FAN_THICK);

      // shift blade so it starts at the hub, not centered on it
      Matrix4f local_shift = make_translate(0.0f, FAN_RADIUS * 0.5f, 0.0f);

      // spin around X axis (the nacelle/facing direction)
      Matrix4f blade_rot;
      blade_rot.InitRotateTransform(blade_angle, 0.0f, 0.0f);

      // rotate the whole spinning assembly to face the road
      Matrix4f face_rot;
      face_rot.InitRotateTransform(0.0f, fan_facing_deg, 0.0f);

      Matrix4f t = make_translate(hub_x, tower_top_y, hub_z);

      Matrix4f model = t * face_rot * blade_rot * local_shift * s;
      upload_model_matrix(model);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 7. gimbal light (elevated on a pole at the front of the building)
    // the pole raises the light above the roof surface
    // the light is offset to the front, not centered on the roof

    // pole (thin grey vertical box)
    float pole_x = bld.x + to_road_x * (bld.width * 0.4f);
    float pole_z = bld.z + to_road_z * (bld.depth * 0.4f);
    float pole_base_y = bld_height;
    float pole_height = 0.5f;

    set_object_color(0.4f, 0.4f, 0.4f);
    set_shininess(0.0f);
    {
      Matrix4f pole = make_srt_3d(0.08f, pole_height, 0.08f, 0, 0, 0, pole_x,
                                  pole_base_y + pole_height / 2.0f, pole_z);
      upload_model_matrix(pole);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // light head (small colored box on top of the pole, swings with the gimbal)
    float light_y = pole_base_y + pole_height + 0.1f;
    set_object_color(bld.light_r, bld.light_g, bld.light_b);
    set_shininess(0.0f);
    {
      Matrix4f light_model =
          make_srt_3d(0.3f, 0.2f, 0.3f, 0, fan_facing_deg + bld.light_swing_deg,
                      0, pole_x, light_y, pole_z);
      upload_model_matrix(light_model);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
  }

  glBindVertexArray(0);
}

// generic car drawing function used for both the player and ai cars.
// i wrote this so i don't have to copy-paste the body+roof+wheel code
// three different times. it takes the position, heading, and body color.
static void draw_car_at(float car_x, float car_z, float heading_deg,
                        float body_r, float body_g, float body_b,
                        bool is_crashed)
{
  glBindVertexArray(g_vao_cube);
  disable_texture(); // use solid colors so the cars don't look like cardboard boxes!

  float heading = heading_deg;
  float car_base_y = CAR_WHEEL_R;

  // 1. main car body 
  // making it shiny so it looks like metal paint
  set_object_color(body_r, body_g, body_b);
  set_shininess(64.0f);

  Matrix4f body = make_srt_3d(CAR_LENGTH, CAR_HEIGHT, CAR_WIDTH, 0, heading, 0,
                              car_x, car_base_y + CAR_HEIGHT / 2.0f, car_z);
  upload_model_matrix(body);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // 2. car roof / windshield
  // i decided to use a very dark tint for the glass to make it look cool
  set_object_color(0.1f, 0.1f, 0.15f); 
  set_shininess(64.0f);

  Matrix4f roof_s;
  roof_s.InitScaleTransform(CAR_ROOF_L, CAR_ROOF_H, CAR_WIDTH * 0.8f);

  Matrix4f roof_local =
      make_translate(0.0f, CAR_HEIGHT / 2.0f + CAR_ROOF_H / 2.0f, 0.0f);

  Matrix4f car_rot;
  car_rot.InitRotateTransform(0.0f, heading, 0.0f);

  Matrix4f car_pos =
      make_translate(car_x, car_base_y + CAR_HEIGHT / 2.0f, car_z);

  Matrix4f roof_model = car_pos * car_rot * roof_local * roof_s;
  upload_model_matrix(roof_model);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // 3. wheels (4 small black cubes, no texture)
  // simple and effective!
  disable_texture();
  set_object_color(0.1f, 0.1f, 0.1f);
  set_shininess(4.0f);

  float wl_offsets[][2] = {
      {CAR_LENGTH * 0.4f, CAR_WIDTH * 0.5f},   // front right
      {CAR_LENGTH * 0.4f, -CAR_WIDTH * 0.5f},  // front left
      {-CAR_LENGTH * 0.4f, CAR_WIDTH * 0.5f},  // back right
      {-CAR_LENGTH * 0.4f, -CAR_WIDTH * 0.5f}, // back left
  };

  for (int w = 0; w < 4; w++)
  {
    Matrix4f ws;
    ws.InitScaleTransform(CAR_WHEEL_R * 2, CAR_WHEEL_R * 2, CAR_WHEEL_R * 2);

    Matrix4f wlocal =
        make_translate(wl_offsets[w][0], -CAR_HEIGHT / 2.0f, wl_offsets[w][1]);

    Matrix4f wheel_model = car_pos * car_rot * wlocal * ws;
    upload_model_matrix(wheel_model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  // 4. front details: headlights and grille
  // adding these makes it actually look like a car!
  set_object_color(1.0f, 0.95f, 0.6f); // bright yellow-white headlights
  set_shininess(128.0f);
  
  // right headlight
  Matrix4f hl_s; 
  hl_s.InitScaleTransform(0.05f, 0.15f, 0.25f);
  Matrix4f hl_local1 = make_translate(CAR_LENGTH * 0.5f, 0.0f, CAR_WIDTH * 0.35f);
  Matrix4f hl_model1 = car_pos * car_rot * hl_local1 * hl_s;
  upload_model_matrix(hl_model1);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  
  // left headlight
  Matrix4f hl_local2 = make_translate(CAR_LENGTH * 0.5f, 0.0f, -CAR_WIDTH * 0.35f);
  Matrix4f hl_model2 = car_pos * car_rot * hl_local2 * hl_s;
  upload_model_matrix(hl_model2);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // front grille (black plastic intake)
  set_object_color(0.05f, 0.05f, 0.05f); 
  set_shininess(4.0f);
  Matrix4f g_s; 
  g_s.InitScaleTransform(0.05f, 0.2f, CAR_WIDTH * 0.45f);
  Matrix4f g_local = make_translate(CAR_LENGTH * 0.5f, -0.05f, 0.0f);
  Matrix4f g_model = car_pos * car_rot * g_local * g_s;
  upload_model_matrix(g_model);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // 5. rear details: taillights
  set_object_color(0.9f, 0.05f, 0.05f); // bright red taillights
  set_shininess(64.0f);
  
  // right taillight
  Matrix4f tl_local1 = make_translate(-CAR_LENGTH * 0.5f, 0.05f, CAR_WIDTH * 0.35f);
  Matrix4f tl_model1 = car_pos * car_rot * tl_local1 * hl_s;
  upload_model_matrix(tl_model1);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  
  // left taillight
  Matrix4f tl_local2 = make_translate(-CAR_LENGTH * 0.5f, 0.05f, -CAR_WIDTH * 0.35f);
  Matrix4f tl_model2 = car_pos * car_rot * tl_local2 * hl_s;
  upload_model_matrix(tl_model2);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // 6. rear spoiler (using same color as the main body)
  set_object_color(body_r, body_g, body_b);
  set_shininess(64.0f);

  // spoiler wing
  Matrix4f sp_s; 
  sp_s.InitScaleTransform(0.2f, 0.05f, CAR_WIDTH * 0.9f);
  Matrix4f sp_local = make_translate(-CAR_LENGTH * 0.45f, CAR_HEIGHT * 0.55f, 0.0f);
  Matrix4f sp_model = car_pos * car_rot * sp_local * sp_s;
  upload_model_matrix(sp_model);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  
  // spoiler struts to connect it to the car
  disable_texture();
  set_object_color(0.1f, 0.1f, 0.1f);
  Matrix4f str_s; 
  str_s.InitScaleTransform(0.05f, 0.15f, 0.05f);
  
  Matrix4f str_local1 = make_translate(-CAR_LENGTH * 0.45f, CAR_HEIGHT * 0.45f, CAR_WIDTH * 0.3f);
  Matrix4f str_model1 = car_pos * car_rot * str_local1 * str_s;
  upload_model_matrix(str_model1);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  
  Matrix4f str_local2 = make_translate(-CAR_LENGTH * 0.45f, CAR_HEIGHT * 0.45f, -CAR_WIDTH * 0.3f);
  Matrix4f str_model2 = car_pos * car_rot * str_local2 * str_s;
  upload_model_matrix(str_model2);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // 7. crash visualization
  // if the car is crashed, draw a guy lying on the ground with blood.
  // this is a fun detail i added to show when a collision happens!
  if (is_crashed)
  {
    disable_texture();

    // calculate a position slightly next to the car door
    float guy_x = car_x + cos((heading_deg + 90.0f) * M_PI / 180.0f) * 1.5f;
    float guy_z = car_z - sin((heading_deg + 90.0f) * M_PI / 180.0f) * 1.5f;
    float floor_y = 0.02f;

    // blood pool (flat red square on the ground)
    set_object_color(0.6f, 0.05f, 0.05f); // dark blood red
    set_shininess(0.0f);
    Matrix4f blood_model =
        make_srt_3d(1.5f, 0.01f, 1.2f, 0, heading_deg + 45.0f, 0,
                    guy_x, floor_y, guy_z);
    upload_model_matrix(blood_model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // dead guy body (lying flat on ground)
    set_object_color(0.2f, 0.3f, 0.8f); // blue shirt
    Matrix4f body_model =
        make_srt_3d(0.8f, 0.2f, 0.4f, 0, heading_deg, 0,
                    guy_x, floor_y + 0.1f, guy_z);
    upload_model_matrix(body_model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // dead guy head
    set_object_color(0.9f, 0.75f, 0.6f); // skin tone
    Matrix4f head_model =
        make_srt_3d(0.3f, 0.2f, 0.3f, 0, heading_deg, 0,
                    guy_x + cos(heading_deg * M_PI / 180.0f) * 0.55f, 
                    floor_y + 0.1f,
                    guy_z - sin(heading_deg * M_PI / 180.0f) * 0.55f);
    upload_model_matrix(head_model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // dead guy legs
    set_object_color(0.3f, 0.3f, 0.3f); // grey pants
    Matrix4f legs_model =
        make_srt_3d(0.7f, 0.15f, 0.35f, 0, heading_deg + 10.0f, 0,
                    guy_x - cos(heading_deg * M_PI / 180.0f) * 0.7f, 
                    floor_y + 0.08f,
                    guy_z + sin(heading_deg * M_PI / 180.0f) * 0.7f);
    upload_model_matrix(legs_model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  glBindVertexArray(0);
}

static void draw_car()
{
  // player car is metallic red
  draw_car_at(g_car.x, g_car.z, g_car.heading_deg, 0.75f, 0.15f,
              0.15f, g_car.collided);
}

static void draw_ai_cars()
{
  for (int i = 0; i < NUM_AI_CARS; i++)
  {
    draw_car_at(g_ai_cars[i].x, g_ai_cars[i].z, g_ai_cars[i].heading_deg,
                g_ai_cars[i].r, g_ai_cars[i].g, g_ai_cars[i].b,
                g_ai_cars[i].stopped);
  }
}

static void draw_street_lights()
{
  disable_texture();
  glBindVertexArray(g_vao_cube);

  for (int i = 0; i < NUM_STREET_LIGHTS; i++)
  {
    float lx = g_street_lights[i].x;
    float lz = g_street_lights[i].z;

    // pole (thin grey vertical box)
    set_object_color(0.45f, 0.45f, 0.45f);
    set_shininess(0.0f);
    Matrix4f pole = make_srt_3d(STREET_LIGHT_POLE_THICK, STREET_LIGHT_HEIGHT,
                                STREET_LIGHT_POLE_THICK, 0, 0, 0, lx,
                                STREET_LIGHT_HEIGHT / 2.0f, lz);
    upload_model_matrix(pole);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // light head (small warm yellow box on top)
    set_object_color(1.0f, 0.9f, 0.5f);
    set_shininess(0.0f);
    Matrix4f head = make_srt_3d(0.3f, 0.15f, 0.3f, 0, 0, 0, lx,
                                STREET_LIGHT_HEIGHT + 0.1f, lz);
    upload_model_matrix(head);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  glBindVertexArray(0);
}

static void draw_celestial()
{
  // draw the sun or moon as it arcs across the sky inside the world.
  // the arc goes from CELESTIAL_START_X to CELESTIAL_END_X (beyond the arena)
  // with height = CELESTIAL_MAX_Y * sin(PI * progress).
  // sun: bigger bright yellow cube. moon: smaller pale white cube.
  // both gently spin around y for visual interest.
  disable_texture();
  glBindVertexArray(g_vao_cube);

  float arc_height = sin(M_PI * g_celestial_progress);
  float cel_x = CELESTIAL_START_X +
                g_celestial_progress * (CELESTIAL_END_X - CELESTIAL_START_X);
  float cel_y = CELESTIAL_MAX_Y * arc_height;
  float cel_z = ARENA_DEPTH * 0.5f;

  if (g_celestial_is_sun)
  {
    // sun: bright yellow cube (smaller than before)
    set_object_color(1.0f, 0.95f, 0.1f);
    set_shininess(0.0f);
    Matrix4f model = make_srt_3d(1.5f, 1.5f, 1.5f,
                                 0, g_celestial_spin_deg, 0,
                                 cel_x, cel_y, cel_z);
    upload_model_matrix(model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }
  else
  {
    // moon: pale white cube
    set_object_color(0.9f, 0.9f, 0.95f);
    set_shininess(0.0f);
    Matrix4f model = make_srt_3d(1.0f, 1.0f, 1.0f,
                                 0, g_celestial_spin_deg, 0,
                                 cel_x, cel_y, cel_z);
    upload_model_matrix(model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  glBindVertexArray(0);
}

static void draw_trees()
{
  // each tree is a brown rectangular trunk with a cluster
  // of green cubes on top as leaves (blocky minecraft style)
  glBindVertexArray(g_vao_cube);

  for (int i = 0; i < NUM_TREES; i++)
  {
    float tx = g_trees[i].x;
    float tz = g_trees[i].z;

    // trunk (wood textured brown box)
    enable_texture(g_tex_buildings[1]); // reuse wood texture for trunk
    set_object_color(0.55f, 0.35f, 0.18f);
    set_shininess(0.0f);
    Matrix4f trunk =
        make_srt_3d(TREE_TRUNK_THICK, TREE_TRUNK_HEIGHT, TREE_TRUNK_THICK, 0, 0,
                    0, tx, TREE_TRUNK_HEIGHT / 2.0f, tz);
    upload_model_matrix(trunk);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // leaves: cluster of green cubes with leaf texture
    enable_texture(g_tex_leaf);
    set_object_color(0.18f, 0.55f, 0.15f);
    float leaf_y = TREE_TRUNK_HEIGHT + 0.4f;
    float leaf_offsets[][3] = {
        {0.0f, 0.0f, 0.0f},   // center
        {0.35f, 0.1f, 0.0f},  // right
        {-0.35f, 0.1f, 0.0f}, // left
        {0.0f, 0.1f, 0.35f},  // front
        {0.0f, 0.3f, 0.0f},   // top
    };

    for (int l = 0; l < 5; l++)
    {
      float lx = tx + leaf_offsets[l][0];
      float ly = leaf_y + leaf_offsets[l][1];
      float lz_pos = tz + leaf_offsets[l][2];

      Matrix4f leaf = make_srt_3d(0.6f, 0.6f, 0.6f, 0, 0, 0, lx, ly, lz_pos);
      upload_model_matrix(leaf);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
  }

  glBindVertexArray(0);
  disable_texture();
}

static void draw_humans()
{
  // each human is a stack of cubes: 2 legs, a body, a head.
  // they stand still, facing each other as if talking.
  //
  // if i want to animate them later, i can:
  //   accumulate a timer (like g_anim_time += dt)
  //   offset leg positions with sin(g_anim_time * 2.0f) * 0.1f
  //   add a small sway to the body with cos(g_anim_time * 1.5f) * 0.05f
  //   this would make them look like they are shifting weight
  //   between their feet, which looks like talking or fidgeting.
  //   the key idea is that sin/cos with different frequencies
  //   create organic looking periodic motion.

  disable_texture();
  glBindVertexArray(g_vao_cube);

  for (int i = 0; i < NUM_HUMANS; i++)
  {
    float hx = g_humans[i].x;
    float hz = g_humans[i].z;
    float facing = g_humans[i].facing_deg;

    Matrix4f facing_rot;
    facing_rot.InitRotateTransform(0.0f, facing, 0.0f);
    Matrix4f base_pos = make_translate(hx, 0.0f, hz);

    // legs (two thin dark boxes)
    set_object_color(0.2f, 0.2f, 0.3f); // dark pants
    set_shininess(0.0f);

    for (int leg = -1; leg <= 1; leg += 2)
    {
      Matrix4f ls;
      ls.InitScaleTransform(0.1f, 0.5f, 0.12f);
      Matrix4f lpos = make_translate(leg * 0.08f, 0.25f, 0.0f);
      Matrix4f model = base_pos * facing_rot * lpos * ls;
      upload_model_matrix(model);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // body (wider box on top of legs)
    set_object_color(g_humans[i].shirt_r, g_humans[i].shirt_g,
                     g_humans[i].shirt_b);
    {
      Matrix4f bs;
      bs.InitScaleTransform(0.3f, 0.45f, 0.18f);
      Matrix4f bpos = make_translate(0.0f, 0.72f, 0.0f);
      Matrix4f model = base_pos * facing_rot * bpos * bs;
      upload_model_matrix(model);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // head (small skin colored cube)
    set_object_color(0.85f, 0.7f, 0.55f);
    {
      Matrix4f hs;
      hs.InitScaleTransform(0.18f, 0.2f, 0.18f);
      Matrix4f hpos = make_translate(0.0f, 1.05f, 0.0f);
      Matrix4f model = base_pos * facing_rot * hpos * hs;
      upload_model_matrix(model);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
  }

  glBindVertexArray(0);
}

// section 13: render frame

// my mental model:
//   update_game() decides "what the world looks like now"
//   render_frame() simply reads that state and sends it to the gpu
//
// i enable depth testing so closer objects occlude farther ones.
// the sky color changes with the day/night cycle.

static void render_frame()
{
  // sky color: blue during day, dark at night
  float arc_h = sin(M_PI * g_celestial_progress);
  float day_factor = g_celestial_is_sun ? arc_h : 0.0f;
  float sky_r = 0.05f + 0.45f * day_factor;
  float sky_g = 0.08f + 0.52f * day_factor;
  float sky_b = 0.15f + 0.65f * day_factor;
  glClearColor(sky_r, sky_g, sky_b, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(g_shader_program);

  setup_projection_matrix();
  setup_view_matrix();
  upload_view_proj();
  upload_lighting();

  // draw every part of the world
  draw_ground();
  draw_track();
  draw_walls();
  draw_buildings();
  draw_car();
  draw_ai_cars();
  draw_street_lights();
  draw_celestial();
  draw_trees();
  draw_humans();

  // imgui heads up display
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Car World HUD");

  const char *cam_names[] = {"Sky View", "Car View", "Ground View",
                             "Light B0", "Light B1", "Light B2",
                             "Light B3", "Light B4", "Helicopter Cam"};
  ImGui::Text("Camera: %s (%d/%d)", cam_names[g_current_camera],
              g_current_camera + 1, NUM_CAMERAS);
  ImGui::Text("Car Speed: %.1f", g_car.speed);
  ImGui::Text("Car Heading: %.1f deg", g_car.heading_deg);
  ImGui::Text("Fan RPM: %.0f", g_fan_rpm);

  // show time of day
  // show time of day based on celestial state
  const char *time_label = g_celestial_is_sun ? "Day" : "Night";
  ImGui::Text("Celestial: %s (%.0f%% across)", time_label,
              g_celestial_progress * 100.0f);

  if (g_car.collided)
  {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                       "COLLISION! Press 'n' to reset");
  }

  ImGui::Separator();

  if (ImGui::CollapsingHeader("Controls"))
  {
    ImGui::Text("f / s          : Accelerate / Decelerate");
    ImGui::Text("l / r          : Steer left / right");
    ImGui::Text("w / W          : Fan speed up / down");
    ImGui::Text("1              : Sky view");
    ImGui::Text("2              : Car view");
    ImGui::Text("3              : Ground view (arrows to pan)");
    ImGui::Text("4 to 8         : Light source view (per building)");
    ImGui::Text("9              : Helicopter cam");
    ImGui::Text("n              : Reset world");
    ImGui::Text("Escape         : Quit");
  }

  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// section 14: input callbacks

// this is how i let the player talk to my game:
//   keyboard controls the car, switches cameras, adjusts fan speed
//   number keys 1 through 9 switch between views
//
// important: when the car is collided, ALL controls are blocked
// (speed, steering). only 'n' to reset and camera/fan controls work.

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods)
{
  (void)scancode;

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
  {
    glfwSetWindowShouldClose(window, GL_TRUE);
    return;
  }

  // camera switching with number keys 1 through 9
  if (action == GLFW_PRESS)
  {
    if (key == GLFW_KEY_1)
      g_current_camera = 0;
    if (key == GLFW_KEY_2)
      g_current_camera = 1;
    if (key == GLFW_KEY_3)
      g_current_camera = 2;
    if (key == GLFW_KEY_4)
      g_current_camera = 3;
    if (key == GLFW_KEY_5)
      g_current_camera = 4;
    if (key == GLFW_KEY_6)
      g_current_camera = 5;
    if (key == GLFW_KEY_7)
      g_current_camera = 6;
    if (key == GLFW_KEY_8)
      g_current_camera = 7;
    if (key == GLFW_KEY_9)
      g_current_camera = 8;
  }

  // car speed controls
  // blocked when collided so the player cant move at all after crash
  if (!g_car.collided)
  {
    if ((key == GLFW_KEY_F) &&
        (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
      g_car.speed += CAR_ACCEL;
      if (g_car.speed > CAR_MAX_SPEED)
        g_car.speed = CAR_MAX_SPEED;
    }

    if ((key == GLFW_KEY_S) &&
        (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
      g_car.speed -= CAR_DECEL;
      if (g_car.speed < -CAR_MAX_SPEED)
        g_car.speed = -CAR_MAX_SPEED;
    }

    // car steering also blocked after collision
    if ((key == GLFW_KEY_L) &&
        (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
      g_car.heading_deg += CAR_STEER_ANGLE;
    }

    if ((key == GLFW_KEY_R) &&
        (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
      g_car.heading_deg -= CAR_STEER_ANGLE;
    }
  }

  // fan speed controls (these work even after collision)
  if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT))
  {
    if (mods & GLFW_MOD_SHIFT)
    {
      g_fan_rpm -= 5.0f;
      if (g_fan_rpm < 0.0f)
        g_fan_rpm = 0.0f;
    }
    else
    {
      g_fan_rpm += 5.0f;
    }
  }

  // ground camera panning with left and right arrow keys
  if (g_current_camera == 2)
  {
    if ((key == GLFW_KEY_LEFT) &&
        (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
      g_ground_cam_pan_deg -= 5.0f;
      if (g_ground_cam_pan_deg < -30.0f)
        g_ground_cam_pan_deg = -30.0f;
    }
    if ((key == GLFW_KEY_RIGHT) &&
        (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
      g_ground_cam_pan_deg += 5.0f;
      if (g_ground_cam_pan_deg > 30.0f)
        g_ground_cam_pan_deg = 30.0f;
    }
  }

  // reset the world with 'n'
  if (key == GLFW_KEY_N && action == GLFW_PRESS)
  {
    init_world();
    std::cout << "world reset!\n";
  }
}

// section 15: initialization and main loop

// the main loop is where everything comes together:
//   while window is open:
//       dt = time since last frame
//       update_game(dt)
//       render_frame()
//       swap buffers and poll input

static bool init_glfw_and_window(int width, int height)
{
  if (!glfwInit())
  {
    cerr << "failed to initialize glfw\n";
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  g_window = glfwCreateWindow(width, height, g_program_title, nullptr, nullptr);
  if (!g_window)
  {
    cerr << "failed to create window\n";
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(g_window);
  glfwSetKeyCallback(g_window, key_callback);

  return true;
}

static bool init_glew()
{
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK)
  {
    cerr << "failed to initialize glew: " << glewGetErrorString(err) << "\n";
    return false;
  }
  cout << "gl version: " << glGetString(GL_VERSION) << "\n";

  glEnable(GL_DEPTH_TEST);

  return true;
}

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  if (!init_glfw_and_window(1280, 720))
  {
    return EXIT_FAILURE;
  }
  if (!init_glew())
  {
    return EXIT_FAILURE;
  }

  // set up shaders, geometry, textures, and the world
  create_shader_program();
  create_geometry();
  load_textures();
  init_world();

  // imgui setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(g_window, true);
  ImGui_ImplOpenGL3_Init("#version 410");

  // main game loop
  while (!glfwWindowShouldClose(g_window))
  {
    float dt = get_delta_time();
    update_game(dt);
    render_frame();

    glfwSwapBuffers(g_window);
    glfwPollEvents();
  }

  // cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwTerminate();

  return EXIT_SUCCESS;
}
