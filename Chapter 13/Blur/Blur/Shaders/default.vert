#version 450

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoords;
layout(location=0) out vec3 NormalW;
layout(location=1) out vec3 PosW;
layout(location=2) out vec2 TexCoords;

struct Light
{
    vec3 Strength;
    float FalloffStart; // point/spot light only
    vec3 Direction;   // directional/spot light only
    float FalloffEnd;   // point/spot light only
    vec3 Position;    // point light only
    float SpotPower;    // spot light only
};
#define MAX_LIGHTS 16


layout (set=0,binding=0) uniform PassCB{
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 invProj;
	mat4 viewProj;
	mat4 invViewProj;
	vec3 gEyePosW;
	float cbPerObjPad1;
	vec2 RenderTargetSize;
	vec2 InvRenderTargetSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
	vec4 gAmbientLight;
	Light gLights[MAX_LIGHTS];
};


layout (set=1, binding=0) uniform ObjectCB{
	mat4 world;	
	mat4 gTexTransform;
};

layout (set=2, binding=0) uniform MaterialCB{
	vec4 gDiffuseAlbedo;
    vec3 gFresnelR0;
    float  gRoughness;
	mat4 gMatTransform;
};

void main(){
	//vec4 posH = vec4(aPos,1.0) * world;
	//gl_Position = posH * viewProj;
	mat4 mvp = viewProj * world;
	gl_Position = mvp * vec4(aPos,1.0);
	NormalW = vec3(world * vec4(aNormal,0.0));
	PosW = vec3(world*vec4(aPos,1.0));
	// Output vertex attributes for interpolation across triangle.
    vec4 texC = gTexTransform  *vec4(aTexCoords, 0.0f, 1.0f);
	
    TexCoords = (gMatTransform*texC).xy;
	//TexCoords= aTexCoords;
}