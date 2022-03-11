#version 450
layout (location=0) in vec3 inPosL;
layout (location=0) out vec4 outFragCol;

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

layout (set=0, binding=0) uniform ObjectCB{
	mat4 world;	
	mat4 texTransform;
	uint materialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};



layout (set=2,binding=0) uniform PassCB{
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



layout (set=4,binding=0) uniform sampler samp;
layout (set=4,binding=1) uniform texture2D textureMap[6];


layout (set=5,binding=0) uniform textureCube cubeMap;

void main(){
	
	 outFragCol = texture(samplerCube(cubeMap,samp),inPosL);
}