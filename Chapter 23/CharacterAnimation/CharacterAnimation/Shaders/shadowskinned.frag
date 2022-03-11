#version 450

layout(location=0) in vec2 inTexC;
layout(location=0) out vec4 outFragColor;

layout (set=0, binding=0) uniform ObjectCB{
	mat4 world;	
	mat4 texTransform;
	uint materialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
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

struct Material
{
    vec4 DiffuseAlbedo;
    vec3 FresnelR0;
    float Shininess;
};
#define MAX_LIGHTS 16


layout (set=2,binding=0) uniform PassCB{
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 invProj;
	mat4 viewProj;
	mat4 invViewProj;
	vec3 gEyePosW;
	mat4 shadowTransform;
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


layout (set=4,binding=0) uniform sampler samp;
layout (set=4,binding=1) uniform texture2D textureMap[6];




layout (constant_id=0) const int NUM_DIR_LIGHTS=3;
layout (constant_id=1) const int enableNormalMap=0;
layout (constant_id=2) const int enableAlphaTest=0;


void main(){
	//MaterialData matData = gMaterialData[gMaterialIndex];
	MaterialData matData = materialData.materials[materialIndex];
	//float4 diffuseAlbedo = matData.DiffuseAlbedo;
	vec4 diffuseAlbedo = matData.DiffuseAlbedo;
	//float3 fresnelR0 = matData.FresnelR0;	
	vec3 fresnelR0 = matData.FresnelR0;
	float roughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;
	
	// Dynamically look up the texture in the array.
	//diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	diffuseAlbedo *= texture(sampler2D(textureMap[diffuseTexIndex],samp),inTexC);
	
	if(enableAlphaTest>0){
		if(diffuseAlbedo.a-0.1f < 0)
			discard;		
	}
	
	
	outFragColor = diffuseAlbedo;
	
}