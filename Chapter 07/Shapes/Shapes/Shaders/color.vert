#version 450

layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
layout(location=0) out vec4 color;



layout (set=0,binding=0) uniform PassCB{
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 invProj;
	mat4 viewProj;
	mat4 invViewProj;
	vec3 EyePosW;
	float cbPerObjPad1;
	vec2 RenderTargetSize;
	vec2 InvRenderTargetSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
};


layout (set=1, binding=0) uniform ObjectCB{
	mat4 world;	
};



void main(){

	//vec4 posH = vec4(aPos,1.0) * world;
	//gl_Position = posH * viewProj;
	mat4 mvp = viewProj * world;
	gl_Position = mvp * vec4(aPos,1.0);
	color = aColor;
}