#version 450

layout(location=0) in vec3 aPosW;
layout(location=1) in vec2 aSizeW;

layout(location=0) out vec3 PosW;
layout(location=1) out vec2 SizeW;

void main(){
	// Just pass data over to geometry shader.
	PosW = aPosW;
	SizeW = aSizeW;
}