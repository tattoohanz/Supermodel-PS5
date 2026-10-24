#version 450

// New3D's translucency composite (R3DFrameBuffers::AllocShaderTrans), drawn over the scene after
// every priority layer: the two translucency layers were drawn without blending, so overlapping
// translucent polygons of one layer (a shadow made of several) replace each other instead of
// darkening again, and only here are they laid over what is behind them.
//
// Written premultiplied for a ONE / ONE_MINUS_SRC_ALPHA blend, which is the SRC_ALPHA blend New3D
// uses for one sample, and lets the multisampled variant average its samples.

layout(location = 0) out vec4 outColour;

#ifdef MULTISAMPLED
layout(set = 0, binding = 0) uniform sampler2DMS uTrans1;
layout(set = 0, binding = 1) uniform sampler2DMS uTrans2;
#else
layout(set = 0, binding = 0) uniform sampler2D uTrans1;
layout(set = 0, binding = 1) uniform sampler2D uTrans2;
#endif

vec4 Combine(vec4 t1, vec4 t2)
{
	// Where both layers overlap the result is opaque.
	if (t1.a * t2.a > 0.0)
		return vec4(mix(t1.rgb, t2.rgb, (t2.a + (1.0 - t1.a)) / 2.0), 1.0);
	if (t1.a > 0.0)
		return t1;
	return t2;
}

void main()
{
	ivec2 p = ivec2(gl_FragCoord.xy);
#ifdef MULTISAMPLED
	int samples = textureSamples(uTrans1);
	vec4 sum = vec4(0.0);
	for (int s = 0; s < samples; s++)
	{
		vec4 c = Combine(texelFetch(uTrans1, p, s), texelFetch(uTrans2, p, s));
		sum += vec4(c.rgb * c.a, c.a);
	}
	vec4 colour = sum / float(samples);
#else
	vec4 c = Combine(texelFetch(uTrans1, p, 0), texelFetch(uTrans2, p, 0));
	vec4 colour = vec4(c.rgb * c.a, c.a);
#endif
	if (colour.a <= 0.0)
		discard;
	outColour = colour;
}
