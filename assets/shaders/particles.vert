#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inVel;
layout(location = 0) out vec2 outCol;

void main()
{
	float green = 1.f - clamp(0.5 * length(inVel), 0.0, 1.0);
	outCol = vec2(1.0, green);
	gl_Position = vec4(inPos, 0.0, 1.0);
	// gl_PointSize = 2.0; // android
}
