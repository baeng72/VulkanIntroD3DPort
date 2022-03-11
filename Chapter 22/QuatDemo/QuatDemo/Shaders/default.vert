#version 450

layout(location=0) in vec3 inPosL;
layout(location=1) in vec3 inNormalL;
layout(location=2) in vec2 inTexC;
layout(location=0) out vec3 outPosW;
layout(location=1) out vec3 outNormalW;
layout(location=2) out vec2 outTexC;

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
	mat4 TexTransform;
	uint MaterialIndex;
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
	uint     MatPos0;
	uint     MatPad1;
	uint     MatPad2;
};

layout (set=2, binding=0) readonly buffer MaterialBuffer{
	MaterialData materials[];
}materialData;

void main(){
	// Fetch the material data.
	//MaterialData matData = gMaterialData[gMaterialIndex];
	MaterialData matData = materialData.materials[MaterialIndex];
	
	// Transform to world space.
    //float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    //vout.PosW = posW.xyz;
	vec4 posW = world * vec4(inPosL,1.0f);
	outPosW = posW.xyz;
	
	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    //vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
	outNormalW = mat3(world)*inNormalL;
	
	// Transform to homogeneous clip space.
    //vout.PosH = mul(posW, gViewProj);
	gl_Position = viewProj * posW;
	
	// Output vertex attributes for interpolation across triangle.
	//float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	//vout.TexC = mul(texC, matData.MatTransform).xy;
	vec4 texC = TexTransform * vec4(inTexC,0.0f,1.0f);
	outTexC = (matData.MatTransform * texC).xy;
	
}