#version 450
layout (location=0) in vec3 inPosL;
layout (location=1) in vec3 inNormal;
layout (location=2) in vec2 inTexC;
layout (location=0) out vec3 outPosL;

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
	mat4 gWorld;	
	mat4 gTexTransform;
	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};

void main(){
	outPosL = inPosL;
	vec4 posW = gWorld * vec4(inPosL,1.0f);
	posW.xyz += gEyePosW;
	gl_Position = (viewProj * posW).xyww;
	 
}