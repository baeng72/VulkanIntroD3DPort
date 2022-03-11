#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inTexC;
layout(location=3) in vec3 inTangentU;
layout(location=0) out vec3 outNormalW;
layout(location=1) out vec3 outTangentW;
layout(location=3) out vec2 outTexC;

layout (set=0, binding=0) uniform ObjectCB{
	mat4 world;	
	mat4 texTransform;
	uint materialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};

layout(set=1,binding=0) uniform SkinnedUBO{
    mat4 boneTransforms[96];
};

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


layout (set=2,binding=0) uniform PassCB{
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 invProj;
	mat4 viewProj;
	mat4 invViewProj;
	mat4 viewProjTex;
	mat4 shadowTransform;
	vec3 eyePosW;
	float cbPerObjPad1;
	vec2 RenderTargetSize;
	vec2 InvRenderTargetSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
	vec4 ambientLight;	
	Light gLights[MAX_LIGHTS];
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

layout (set=3, binding=0) readonly buffer MaterialBuffer{
	MaterialData materials[];
}materialData;

void main(){
	MaterialData matData = materialData.materials[materialIndex];
	
	
	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    outNormalW = vec3(world * vec4(inNormal,0.0));
	outTangentW = vec3(world*vec4(inTangentU,0.0));
	
	// Transform to homogeneous clip space.
    //float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    //vout.PosH = mul(posW, gViewProj);
	vec4 posW = world * vec4(inPos,1.0f);
	gl_Position = viewProj * posW;
	
	// Output vertex attributes for interpolation across triangle.
	//float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	//vout.TexC = mul(texC, matData.MatTransform).xy;
	vec4 texC = texTransform * vec4(inTexC,0.0f,1.0f);
	outTexC = (matData.MatTransform*texC).xy;
	
 }