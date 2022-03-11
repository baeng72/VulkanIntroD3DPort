#version 450

layout (location=0) in vec3 aPos;
layout (location=1) in vec4 aColor;

layout (location=0) out vec4 Color;

layout (binding=0) uniform UBOP{
	mat4 WorldViewProj;
};

void main(){
	gl_Position = WorldViewProj * vec4(aPos,1.0f);
	Color = aColor;
}