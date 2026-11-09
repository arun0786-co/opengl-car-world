// fragment shader for the 3d car world
//
// this shader receives the interpolated normal, world position,
// and texture coordinate from the vertex shader and computes
// per pixel lighting.
//
// lighting model (phong style):
//
//   ambient:  a small constant light so nothing is fully black
//
//   diffuse:  light intensity based on the angle between the surface
//             normal and the light direction (dot product). surfaces
//             facing the light are bright, surfaces away are dark.
//
//   specular: bright highlights on shiny surfaces like the car body.
//             uses the blinn phong halfway vector approach.
//
// light sources:
//
//   one directional light simulating the sun, shining from above.
//   the sun moves across the sky so its direction changes each frame.
//   at night the sun dips below the horizon and its contribution
//   drops to zero, leaving only point lights.
//
//   up to 16 point lights:
//     5 building top gimbal lamps (colored, swiveling)
//     8 street light poles along the road (warm white, fixed)
//   these add colored illumination to nearby surfaces and
//   fall off with distance.
//
// texture mapping:
//   when uUseTexture is 1, i sample a 2d texture at the fragment's
//   uv coordinate and use that color instead of the solid uColor.
//   this is how building walls get brick/wood/concrete patterns.
//   when uUseTexture is 0, i use the solid uColor (for cars, road, etc).

#version 410

layout(location = 0) out vec4 diffuseColor;

// per object base color sent from the cpu via glUniform4f
uniform vec4 uColor;

// texture mapping
uniform sampler2D uTexture;
uniform int uUseTexture;

// directional light (sun)
uniform vec3 uLightDir;      // direction of the main directional light (normalized)
uniform vec3 uLightColor;    // color and intensity of the directional light

// camera position for specular highlights
uniform vec3 uCameraPos;

// material shininess: higher value means tighter specular highlights.
// the car uses a high value (metallic), buildings use a low value.
uniform float uShininess;

// point light uniforms for building lamps and street lights
// i support up to 16 point lights total
const int MAX_POINT_LIGHTS = 16;
uniform int uNumPointLights;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];

// inputs from vertex shader (interpolated across the triangle)
in vec3 fragNormal;
in vec3 fragWorldPos;
in vec2 fragTexCoord;

void main()
{
    // normalize the interpolated normal because interpolation
    // between vertices can change its length
    vec3 normal = normalize(fragNormal);

    // decide the base surface color:
    // if a texture is bound, sample it at the fragment's uv coord.
    // otherwise use the solid uniform color.
    vec3 baseColor;
    if (uUseTexture == 1)
    {
        baseColor = texture(uTexture, fragTexCoord).rgb;
    }
    else
    {
        baseColor = uColor.rgb;
    }

    // ambient component
    // a small constant light so objects in shadow are not pitch black.
    // keeping this low so the lighting differences are visible.
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * uLightColor;

    // diffuse component from the directional light (sun)
    // the amount of light hitting the surface depends on the angle
    // between the surface normal and the light direction.
    // dot(normal, lightDir) gives cos(angle):
    //   1 when the surface directly faces the light
    //   0 when the surface is perpendicular
    // max() clamps negative values so back faces get zero light.
    vec3 lightDir = normalize(uLightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;

    // specular component from the directional light (blinn phong)
    // this creates bright shiny highlights on metallic surfaces.
    // i compute the halfway vector between the light direction and
    // the view direction, then raise the dot product of that with
    // the normal to the power of shininess.
    // for non shiny objects like buildings and road, uShininess is 0
    // so this calculation is skipped.
    vec3 viewDir = normalize(uCameraPos - fragWorldPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = 0.0;
    if (uShininess > 0.0)
    {
        spec = pow(max(dot(normal, halfwayDir), 0.0), uShininess);
    }
    vec3 specular = spec * uLightColor;

    // point lights from building top lamps and street lights
    // each point light adds diffuse and specular based on distance.
    // i use simple attenuation: 1 / (1 + distance^2 * factor)
    // so lights get dimmer with distance, which looks natural.
    vec3 pointLightResult = vec3(0.0);
    for (int i = 0; i < uNumPointLights; i++)
    {
        vec3 plDir = uPointLightPos[i] - fragWorldPos;
        float plDist = length(plDir);
        plDir = normalize(plDir);

        // attenuation: light intensity falls off with distance squared
        float attenuation = 1.0 / (1.0 + plDist * plDist * 0.05);

        // diffuse from this point light
        float plDiff = max(dot(normal, plDir), 0.0);
        pointLightResult += plDiff * uPointLightColor[i] * attenuation;

        // specular from this point light (only for shiny objects)
        if (uShininess > 0.0)
        {
            vec3 plHalf = normalize(plDir + viewDir);
            float plSpec = pow(max(dot(normal, plHalf), 0.0), uShininess);
            pointLightResult += plSpec * uPointLightColor[i] * attenuation;
        }
    }

    // combine all lighting components with the base surface color
    vec3 result = (ambient + diffuse + specular + pointLightResult) * baseColor;

    // clamp to [0, 1] to prevent overly bright pixels
    result = clamp(result, 0.0, 1.0);

    diffuseColor = vec4(result, uColor.a);
}
