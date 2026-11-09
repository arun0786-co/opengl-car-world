// vertex shader for the 3d car world
//
// every vertex passes through three matrix multiplications,
// which is the standard model view projection (mvp) pipeline:
//
//   1. gModel  places the individual object in the 3d world
//              (scale the unit cube into a car body, a building, a wall, etc.
//               rotate the windmill fan, translate to the object's position)
//
//   2. gView   moves the entire world so the camera is at the origin
//              looking down negative z. this is how i implement sky view,
//              car view, ground view, helicopter cam, etc.
//
//   3. gProj   applies perspective projection so things farther from
//              the camera appear smaller. without this the scene would
//              look flat and orthographic.
//
// on top of that, i also pass a normal vector and world position
// to the fragment shader so it can do lighting calculations.
// each face of a cube has a flat outward normal. the fragment shader
// uses the normal to compute how much light hits the surface (diffuse)
// and where shiny highlights appear (specular, for the car body).
//
// i also pass texture coordinates (uv) so that when a building face
// is drawn, the fragment shader can sample a brick/wood/concrete
// texture and color the surface with that pattern. objects that dont
// use textures (car, road, walls) just ignore the uv in the shader.
//
// the final clip space position is:
//   gl_Position = gProj * gView * gModel * vec4(Position, 1.0)

#version 410

// the vertex position coming from my vao/vbo (attribute location 0)
layout(location = 0) in vec3 Position;

// the vertex normal coming from my vao/vbo (attribute location 1)
// i use this for shading calculations in the fragment shader.
// each face of my cube will have a normal pointing outward.
layout(location = 1) in vec3 Normal;

// texture coordinate from my vao/vbo (attribute location 2)
// each face of the cube maps uv from (0,0) to (1,1) so the
// texture covers the entire face. for objects that dont use
// textures the shader just ignores this value.
layout(location = 2) in vec2 TexCoord;

// three separate transformation matrices sent from the cpu
uniform mat4 gModel;
uniform mat4 gView;
uniform mat4 gProj;

// outputs to the fragment shader (interpolated across the triangle)
out vec3 fragNormal;     // normal in world space, for lighting
out vec3 fragWorldPos;   // position in world space, for point lights
out vec2 fragTexCoord;   // texture coordinate, for building textures

void main()
{
    // transform position through the full mvp pipeline:
    // model space then world space then camera space then clip space
    gl_Position = gProj * gView * gModel * vec4(Position, 1.0);

    // transform normal to world space.
    // i use mat3(gModel) to strip the translation part.
    // if the model matrix has non uniform scaling, the proper way is
    // to use the inverse transpose: mat3(transpose(inverse(gModel)))
    // but for this project most objects use axis aligned scaling so
    // mat3(gModel) is accurate enough and avoids the extra gpu cost.
    fragNormal = mat3(gModel) * Normal;

    // world space position for point light distance calculations
    // i multiply position by gModel only (no view or projection)
    fragWorldPos = vec3(gModel * vec4(Position, 1.0));

    // pass through the texture coordinate unchanged.
    // the gpu will interpolate it across the triangle for us.
    fragTexCoord = TexCoord;
}
