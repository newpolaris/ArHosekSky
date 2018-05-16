//------------------------------------------------------------------------------

-- Compute

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(rgba16f, binding=0) uniform readonly image2D uTexSource;
layout(rgba16f, binding=1) uniform writeonly image2D uTexTarget;

float Luminance(vec3 color)
{
    return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

void main()
{
	uint x = gl_GlobalInvocationID.x;	
	uint y = gl_GlobalInvocationID.y;
	ivec2 s = imageSize(uTexSource);

	// check out of bounds
	if (x >= s.x || y >= s.y)
		return;

    vec3 color = imageLoad(uTexSource, ivec2(x, y)).rgb;
    float logLuminance = log(max(Luminance(color), 0.00001f));
	imageStore(uTexTarget, ivec2(x, y), vec4(logLuminance));
}