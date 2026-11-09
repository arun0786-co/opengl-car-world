# How it works

Everything is in `src/main.cpp` and the two shaders. Each frame updates the world state (`update_game`), then draws it (`render_frame`). This document follows the frame from geometry to pixels, then covers the parts added after the course: shadows, spotlights, collisions and headless capture.

## 1. One cube, many model matrices

The only mesh is a unit cube centred at the origin: 36 vertices, each with a position, a normal and a texture coordinate. Every visible object is one or more draws of that cube under a model matrix:

    model = translate * rotate * scale

Scale stretches the cube into a wall, a car body or a blade. Rotate turns it. Translate places it.

Hierarchical objects multiply matrices from parent to child. A windmill blade is

    translate(hub) * rotate_y(face the road) * rotate_x(spin) * translate(0, r/2, 0) * scale(blade)

Read right to left: shape the blade, move it so it starts at the hub, spin it about the hub axis, turn the whole rotor towards the road, then put it on the roof. A car's roof and wheels use the car's translate and rotate, followed by their own offset and scale, so they move with the body.

Normals are transformed with `mat3(model)`. In general that is wrong under non-uniform scaling, which needs the inverse transpose. Here it is exact: every scale is applied first, directly to the cube, and a cube's face normals lie on the axes, so the scale only changes their length.

## 2. The track

The road follows a perturbed ellipse around the arena centre (cx, cz):

    a(t) = 14 + 2.5 sin 2t + 1.8 cos 3t
    b(t) = 11 + 2.0 sin 3t + 1.5 cos 2t
    x(t) = cx + (a(t) - w/2) cos t
    z(t) = cz + (b(t) - w/2) sin t

Here w is the road width. The road is 400 flat boxes placed along the curve, each turned to the tangent and overlapping its neighbours so the curve has no gaps. The tangent comes from a central difference of the curve. The AI cars follow the same curve with a lane offset along the normal, and their heading comes from the tangent.

## 3. Lighting

The fragment shader uses Blinn-Phong shading with these terms:

- **Ambient:** 0.15 times the sun colour.
- **Sun:** a directional light, with diffuse `max(n.l, 0)` and specular `max(n.h, 0)^shininess`, where h is the half vector between the light and view directions. Only the car bodies are shiny.
- **Spotlights:** 13 of them, each attenuated by `1 / (1 + 0.05 d^2)`.

The sun and the moon share one path. A progress value p runs from 0 to 1 across the sky:
- the body's height is `20 sin(pi p)`;
- its x position goes from 30 units beyond one wall to 30 units beyond the other;
- the light direction points from the arena centre towards the body;
- the sun's colour scales with `sin(pi p)`;
- the moon is the same light, dimmer and bluer: (0.22, 0.26, 0.40) against the sun's (1.0, 0.95, 0.85).

## 4. Shadows

A fragment is in shadow when something lies between it and the sun. Shadow mapping answers that with a depth image taken from the sun.

**Pass 1, from the sun.** The sun gets an orthographic camera, since its rays are parallel:
1. Its view matrix looks along the light direction.
2. To fit the projection, the 8 corners of the arena's bounding box (42 x 10 x 42 units) are transformed into the sun's view space. Their min and max on each axis give the box the projection maps to [-1, 1]:

        x' = 2 (x - l) / (r - l) - 1    (same for y with b, t and z with n, f)

   Fitting to the scene keeps texels small. With the sun overhead, one texel of the 4096 x 4096 depth texture covers about 0.01 units of ground (a car is 2 units long).
3. The scene is drawn into the depth texture with the main shader program. A `uDepthPass` uniform makes the fragment shader return at once, because only depth is written. Reusing the program means none of the drawing code had to change.

**Pass 2, from the camera.** The vertex shader also outputs each vertex in the sun's clip space (`uLightVP * world position`). The fragment shader maps that to [0, 1] texture coordinates and compares the fragment's depth with the stored one.

**Acne and bias.** The depth texture stores the depth at texel centres, so a lit surface compares against samples of itself that are slightly nearer or farther. The result is a pattern of false shadow stripes. Two offsets remove it:
- `glPolygonOffset(2, 4)` during pass 1 pushes stored depths away from the sun, more on steep slopes;
- the shader subtracts a bias that grows as the surface turns away from the sun, `mix(0.0015, 0.0004, n.l)`.

Too much bias detaches shadows from their objects ("peter panning"). These values were chosen by looking at the car and lamp posts, where it shows first.

**Soft edges.** The depth texture uses `GL_COMPARE_REF_TO_TEXTURE` with linear filtering. Each `texture()` call on the `sampler2DShadow` then returns the fraction of the 4 nearest texels that pass the depth test, filtered in hardware. The shader averages 9 such taps on a 3 x 3 grid (percentage-closer filtering), so each edge blends over about 16 texels.

**One driver detail.** In pass 1 the depth texture is the render target, so it cannot also be bound for sampling. Leaving texture unit 1 empty made the macOS driver log `unit 1 GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Depth)`. A 1 x 1 depth texture stays bound to unit 1 during pass 1 instead.

The shadow pass draws the whole scene a second time: about 810 extra draw calls and 0.84 ms per frame on an M4 Pro.

## 5. Spotlights

The assignment asks for a lamp on each building that swings 30 degrees either way and points at the nearest point of the road. Originally the lamps were point lights, the same in every direction, so the swing changed nothing in the lighting. Now each lamp is a spotlight:

- At start-up the closest point of the road centre line is found by sampling the curve at 2,000 values of t. That is the lamp's rest aim.
- Each frame, the horizontal part of the aim is turned by the current swing angle.
- The shader fades the light between an inner and an outer cone:

        spot = smoothstep(cos(outer), cos(inner), dot(-L, axis))

  where L is the direction from the fragment to the lamp. The lamps use 10 and 16 degrees, at 3 times their old intensity.

The street lamps are wide spotlights (35 and 60 degrees) pointing down, which gives the pools of light in the night image.

## 6. Cameras

Every camera is an eye position and a forward direction. `InitCameraTransform` builds a rotation from them, which is multiplied by a translation of `-eye`:

| key | camera | eye and direction |
|---|---|---|
| 1 | sky | high above the centre, looking down |
| 2 | car | on the roof, along the heading, tilted down a little |
| 3 | ground | fixed in front of building 0, facing the road, panned by up to 30 degrees |
| 4 to 8 | lamp views | in front of each lamp, turned by the lamp's swing |
| 9 | helicopter | 8 units behind and 5 above the car, looking at it |
| 0 | orbit | on a 34-unit circle at height 20, looking at the arena centre |

## 7. Collisions

Seen from above, every object is a rectangle: a centre, half sizes and a rotation. Two convex shapes are disjoint exactly when some axis separates their projections (the separating axis theorem). For two rectangles it is enough to test the four edge normals. On each axis n:

    |(c_b - c_a) . n|  >=  sum over both rectangles and both local axes u of  h_u |u . n|

If that holds for some n, the rectangles are apart. The wall test uses the car's axis-aligned extent, `h_x |cos| + h_z |sin|` along x and `h_x |sin| + h_z |cos|` along z.

The original checks compared the car's half length along x and half width along z whatever its heading. This caused three problems:
- A car turned 90 degrees could drive its nose into a building.
- At some headings it stopped beside a building it was not touching.
- The wall test used `max(half length, half width)` as the car's reach, but at 45 degrees a corner reaches `(1 + 0.5) / sqrt(2) = 1.06`, so corners went into the wall.

`tests/test_collide.cpp` checks these three cases. It also compares the test with a plain polygon intersection (edge crossings plus point-in-polygon) on 200,000 random pairs. All of them agree.

## 8. Headless frames

With `--shot` or `--bench` the GLFW window is created hidden. The frame is drawn into a framebuffer object with an RGBA8 colour renderbuffer and a 24-bit depth renderbuffer, then read back with `glReadPixels`. OpenGL rows start at the bottom, so they are flipped. `src/png.h` writes the PNG directly:
- the 8-byte signature,
- an `IHDR` chunk (size, 8-bit RGB),
- one `IDAT` chunk holding the zlib-compressed rows, each prefixed by filter byte 0,
- `IEND`.

Every chunk ends with a CRC-32 of its type and data. zlib provides both the compression and the CRC.

The world is stepped at a fixed 1/60 s before the frame is taken, so the same flags always give the same image.

On Linux CI the same binary runs under `xvfb-run` with Mesa's software rasterizer (`LIBGL_ALWAYS_SOFTWARE=1`). The rendered frame is uploaded as a build artifact.

## 9. Fixed on the way

- **Viewport.** The original code never called `glViewport`, so it kept the viewport set when the context was created. OpenGL does not change that when the window is resized, while the projection did follow the new size. The viewport is now set every frame, which the offscreen and shadow passes need anyway.
- **Collisions** (section 7).
- **Lamps that did not point anywhere** (section 5).
