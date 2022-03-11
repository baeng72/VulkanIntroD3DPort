#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inTexC;
layout(location=3) in vec3 inTangentU;
layout(location=0) out vec3 outPosW;
layout(location=1) out vec4 outShadowPosH;
layout(location=2) out vec4 outSsaoPosH;
layout(location=3) out vec3 outNormalW;
layout(location=4) out vec3 outTangentW;
layout(location=5) out vec2 outTexC;

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
	mat4 ViewProjTex;
	mat4 shadowTransform;
	vec3 gEyePosW;
	float cbPerObjPad1;
	vec2 RenderTargetSize;
	vec2 InvRenderTargetSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
	vec4 gAmbientLight;
	// Allow application to change fog parameters once per frame.
	// For example, we may only use fog for certain times of day.
	vec4 gFogColor;
	float gFogStart;
	float gFogRange;
	vec2 cbPerObjectPad2;
	Light gLights[MAX_LIGHTS];
};


layout (set=1, binding=0) uniform ObjectCB{
	mat4 world;	
	mat4 gTexTransform;
	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};

struct MaterialData
{
	vec4   DiffuseAlbedo;
	vec3   FresnelR0;
	float    Roughness;
	mat4 MatTransform;
	uint     DiffuseMapIndex;
	uint     NormalMapIndex;
	uint     MatPad1;
	uint     MatPad2;
};

layout (set=2, binding=0) readonly buffer MaterialBuffer{
	MaterialData materials[];
}materialData;

void main(){
	MaterialData matData = materialData.materials[gMaterialIndex];
	//vec4 posH = vec4(aPos,1.0) * world;
	//gl_Position = posH * viewProj;
	mat4 mvp = viewProj * world;
	gl_Position = mvp * vec4(inPos,1.0);
	outNormalW = vec3(world * vec4(inNormal,0.0));
	outTangentW = vec3(world*vec4(inTangentU,0.0));
	
	 
	outPosW = vec3(world*vec4(inPos,1.0));
	// Output vertex attributes for interpolation across triangle.
    vec4 texC = gTexTransform  *vec4(inTexC, 0.0f, 1.0f);
	
    outTexC = (matData.MatTransform*texC).xy;
	//TexCoords= aTexCoords;
	
	 // Generate projective tex-coords to project SSAO map onto scene.
    //vout.SsaoPosH = mul(posW, gViewProjTex);
	outSsaoPosH = ViewProjTex * vec4(outPosW,1.0);
	
	// Generate projective tex-coords to project shadow map onto scene.
    outShadowPosH = shadowTransform*vec4(outPosW,1.0);//mul(posW, gShadowTransform);
}