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
	vec2 DisplacementMapTexelSize;
	float GridSpatialStep;
	float Pad;
};

layout (set=2, binding=0) uniform MaterialCB{
	vec4 gDiffuseAlbedo;
    vec3 gFresnelR0;
    float  gRoughness;
	mat4 gMatTransform;
};

layout (set=4,binding=0,r32f) uniform readonly image2D displacementMap;

void main(){
	//displacementMap stuff
	vec2 tex = aTexCoords.xy*256;//tex coords are in [0-1] space, transform to [0-256] space, or change from image2D to sampler2D
	float yDisp = imageLoad(displacementMap,ivec2(tex)).r;
	vec3 pos = vec3(aPos.x,aPos.y + yDisp,aPos.z);
	// Estimate normal using finite difference.
	float du = DisplacementMapTexelSize.x;
	float dv = DisplacementMapTexelSize.y;
	float l = imageLoad(displacementMap,ivec2(aTexCoords-vec2(du,0.0f))).r;
	float r = imageLoad(displacementMap,ivec2(aTexCoords+vec2(du, 0.0f))).r;
	float t = imageLoad(displacementMap,ivec2(aTexCoords-vec2(0.0f, dv))).r;
	float b = imageLoad(displacementMap,ivec2(aTexCoords+vec2(0.0f, dv))).r;
	vec3 normal = normalize( vec3(-r+l, 2.0f*GridSpatialStep, b-t) );
	//vec4 posH = vec4(aPos,1.0) * world;
	//gl_Position = posH * viewProj;
	mat4 mvp = viewProj * world;
	
	gl_Position = mvp * vec4(pos,1.0);
	NormalW = vec3(world * vec4(normal,0.0));
	PosW = vec3(world*vec4(pos,1.0));
	// Output vertex attributes for interpolation across triangle.
    vec4 texC = gTexTransform  *vec4(aTexCoords, 0.0f, 1.0f);
	
    TexCoords = (gMatTransform*texC).xy;
	//TexCoords= aTexCoords;
}