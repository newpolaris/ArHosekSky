-- Vertex

// IN
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexcoords;

// Out
out vec3 vTexcoords;

uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    // Rotate into view-space, centered on the camera
    vec3 positionVS = mat3(uView)*inPosition;

    gl_Position = uProjection*vec4(positionVS, 1.0);

    // Make a texture coordinate
	vTexcoords = inPosition;
}

-- Fragment

uniform samplerCube uTexSource;

// IN
in vec3 vTexcoords;

// OUT
out vec3 fragColor;

// ----------------------------------------------------------------------------
void main() 
{
    fragColor = texture(uTexSource, normalize(vTexcoords)).rgb;
}
