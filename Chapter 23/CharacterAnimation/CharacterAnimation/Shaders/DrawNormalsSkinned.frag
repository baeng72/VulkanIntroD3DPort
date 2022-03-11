#version 450
//#define GL_EXT_samplerless_texture_functions

layout(location=0) in vec3 inNormalW;
layout(location=1) in vec3 inTangentW;
layout(location=3) in vec2 inTexC;

layout(location=0) out vec4 outFragColor;

layout (set=0, binding=0) uniform ObjectCB{
	mat4 world;	
	mat4 gTexTransform;
	uint gMaterialIndex;
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



layout (constant_id=0) const int enableAlphaTest=0;

void main(){
	//MaterialData matData = gMaterialData[gMaterialIndex];
	MaterialData matData = materialData.materials[gMaterialIndex];
	//float4 diffuseAlbedo = matData.DiffuseAlbedo;
	vec4 diffuseAlbedo = matData.DiffuseAlbedo;
	//float3 fresnelR0 = matData.FresnelR0;	
	vec3 fresnelR0 = matData.FresnelR0;
	float roughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;
	uint normalMapIndex = matData.NormalMapIndex;
	
	// Dynamically look up the texture in the array.
	//diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	diffuseAlbedo *= texture(sampler2D(textureMap[diffuseTexIndex],samp),inTexC);
	
	if(enableAlphaTest>0){
		if(diffuseAlbedo.a -0.1 < 0.0)
			discard;
	}
	
	
	// Interpolating normal can unnormalize it, so renormalize it.
    //pin.NormalW = normalize(pin.NormalW);
	vec3 norm = normalize(inNormalW);
	  // NOTE: We use interpolated vertex normal for SSAO.

    // Write normal in view space coordinates
    //float3 normalV = mul(pin.NormalW, (float3x3)gView);
	vec3 normalV = mat3(view)*norm;
	
	outFragColor = vec4(normalV,0.0f);
}