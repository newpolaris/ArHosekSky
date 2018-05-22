//------------------------------------------------------------------------------

-- Compute

const uint GroupSize = 8;
const uint NumThreads = GroupSize * GroupSize;

layout(local_size_x = GroupSize, local_size_y = GroupSize, local_size_z = 1) in;
layout(r16f, binding=0) uniform readonly image2D uSource;
layout(r16f, binding=1) uniform writeonly image2D uTarget;

shared float LumSample[NumThreads];

float Luminance(vec3 color)
{
    return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

void main()
{
	uint x = gl_GlobalInvocationID.x;	
	uint y = gl_GlobalInvocationID.y;
	ivec2 s = imageSize(uSource);

	// check out of bounds
	if (x >= s.x || y >= s.y)
		return;

    uint si = gl_LocalInvocationIndex;

    float lum = imageLoad(uSource, ivec2(x, y)).r;
    LumSample[si] = lum;

    // Ensure shared memory writes are visible to work group
    memoryBarrierShared();

    // Ensure all threads in work group   
    // have executed statements above
    barrier();

    for (uint s = NumThreads / 2; s > 0; s >>= 1)
    {
        if (si < s)
            LumSample[si] += LumSample[si + s];
        memoryBarrierShared();
        barrier();
    }

    if (si == 0)
    {
        float avgLuma = LumSample[0]/NumThreads;
        imageStore(uTarget, ivec2(gl_WorkGroupID.xy), vec4(avgLuma));
    }
}