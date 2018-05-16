-- Vertex 

#include "Fullscreen.glsli"

-- Fragment

// In
in vec2 vTexcoords;

// Out
out vec3 fragColor;

uniform sampler2D uTexSource;

// ----------------------------------------------------------------------------
void main() 
{
	vec4 reds = textureGather(uTexSource, vTexcoords, 0);
	vec4 greens = textureGather(uTexSource, vTexcoords, 1);
	vec4 blues = textureGather(uTexSource, vTexcoords, 2);

    vec3 result = vec3(0.0);
    for (uint i = 0; i < 4; ++i)
    {
        vec3 color = vec3(reds[i], greens[i], blues[i]);
        result += color;
    }
    fragColor = result*0.25;
}