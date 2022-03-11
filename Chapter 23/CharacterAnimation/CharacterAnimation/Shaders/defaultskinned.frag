#version 450
//#define GL_EXT_samplerless_texture_functions

layout(location=0) in vec3 inPosW;
layout(location=1) in vec4 inShadowPosH;
layout(location=2) in vec4 inSsaoPosH;
layout(location=3) in vec3 inNormalW;
layout(location=4) in vec3 inTangentW;
layout(location=5) in vec2 inTexC;
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
	mat4 viewProjTex;
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
layout (set=4,binding=1) uniform texture2D textureMap[16];

layout (set=5,binding=0) uniform textureCube cubeMap;

layout (set=6,binding=0) uniform sampler2D shadowMap;

layout (set=7,binding=0) uniform sampler2D ssaoMap;

layout (constant_id=0) const int NUM_DIR_LIGHTS=3;
layout (constant_id=1) const int enableNormalMap=0;
layout (constant_id=2) const int enableAlphaTest=0;
layout (constant_id=3) const int enableSsao=1;

//#define NUM_DIR_LIGHTS 3
//#define NUM_POINT_LIGHTS 0
//#define NUM_SPOT_LIGHTS 0

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    // Linear falloff.
    //return saturate((falloffEnd-d) / (falloffEnd - falloffStart));
	return clamp((falloffEnd-d)/(falloffEnd-falloffStart),0,1);
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
vec3 SchlickFresnel(vec3 R0, vec3 normal, vec3 lightVec)
{
    //float cosIncidentAngle = saturate(dot(normal, lightVec));
	float cosIncidentAngle = clamp(dot(normal, lightVec),0,1);

    float f0 = 1.0f - cosIncidentAngle;
    vec3 reflectPercent = R0 + (1.0f - R0)*(f0*f0*f0*f0*f0);

    return reflectPercent;
}

vec3 BlinnPhong(vec3 lightStrength, vec3 lightVec, vec3 normal, vec3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    vec3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f)*pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    vec3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    vec3 specAlbedo = fresnelFactor*roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
vec3 ComputeDirectionalLight(Light L, Material mat, vec3 normal, vec3 toEye)
{
    // The light vector aims opposite the direction the light rays travel.
    vec3 lightVec = -L.Direction;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    vec3 lightStrength = L.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
//---------------------------------------------------------------------------------------
vec3 ComputePointLight(Light L, Material mat, vec3 pos, vec3 normal, vec3 toEye)
{
    // The vector from the surface to the light.
    vec3 lightVec = L.Position - pos;

    // The distance from surface to light.
    float d = length(lightVec);

    // Range test.
    if(d > L.FalloffEnd)
        return vec3(0.0);

    // Normalize the light vector.
    lightVec /= d;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    vec3 lightStrength = L.Strength * ndotl;

    // Attenuate light by distance.
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for spot lights.
//---------------------------------------------------------------------------------------
vec3 ComputeSpotLight(Light L, Material mat, vec3 pos, vec3 normal, vec3 toEye)
{
    // The vector from the surface to the light.
    vec3 lightVec = L.Position - pos;

    // The distance from surface to light.
    float d = length(lightVec);

    // Range test.
    if(d > L.FalloffEnd)
        return vec3(0.0);

    // Normalize the light vector.
    lightVec /= d;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    vec3 lightStrength = L.Strength * ndotl;

    // Attenuate light by distance.
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    // Scale by spotlight
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//vec3 computeDirLights(Material mat, vec3 normal, vec3 toEye,vec3 shadowFactor){
	//vec3 result=vec3(0.0);
	//for(int i=0;i<NUM_DIR_LIGHTS;++i){
		//result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
//	}
//	return result;
//}

vec4 ComputeLighting( Material mat,
                       vec3 pos, vec3 normal, vec3 toEye,
					   vec3 shadowFactor)
{
    vec3 result = vec3(0.0);

    int i = 0;
   
	for(int i=0;i<NUM_DIR_LIGHTS;++i){
		result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
	}
	
  //  for(i = 0; i < NUM_DIR_LIGHTS; ++i)
  //  {
  //      result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
  //  }


//#if (NUM_POINT_LIGHTS > 0)
    //for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i)
    //{
      //  result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    //}
//#endif

//#if (NUM_SPOT_LIGHTS > 0)
  //  for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    //{
      //  result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
//    }
//#endif 

    return vec4(result, 0.0f);
}

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
vec3 NormalSampleToWorldSpace(vec3 normalMapSample, vec3 unitNormalW, vec3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
	vec3 normalT = 2.0f*normalMapSample - vec3(1.0f);

	// Build orthonormal basis.
	vec3 N = unitNormalW;
	vec3 T = normalize(tangentW - dot(tangentW, N)*N);
	vec3 B = cross(N, T);

	mat3 TBN = mat3(T, B, N);

	// Transform from tangent space to world space.
	vec3 bumpedNormalW = TBN * normalT;// mul(normalT, TBN);

	return bumpedNormalW;
}

float CalcShadowFactor(vec4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    //uint width, height, numMips;
    //gShadowMap.GetDimensions(0, width, height, numMips);
	ivec2 texSize = textureSize(shadowMap,0);
	uint width = texSize.x;
	uint height = texSize.y;

    // Texel size.
    float dx = 1.0f / float(width);

    float percentLit = 0.0f;
    const vec2 offsets[9] =
    {
        vec2(-dx,  -dx), vec2(0.0f,  -dx), vec2(dx,  -dx),
        vec2(-dx, 0.0f), vec2(0.0f, 0.0f), vec2(dx, 0.0f),
        vec2(-dx,  +dx), vec2(0.0f,  +dx), vec2(dx,  +dx)
    };

    //[unroll]
    for(int i = 0; i < 9; ++i)
    {
		float shadow=1.0;
        //percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
          //  shadowPosH.xy + offsets[i], depth).r;
		
			float dist = texture(shadowMap,vec2(shadowPosH.st+offsets[i])).r;
			if(dist < depth)
				shadow = dist;//texture(shadowMap,vec2(shadowPosH.xy+offsets[i])).r;
		
		percentLit += shadow;
		   
    }
    
    return percentLit / 9.0f;
}

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = dist;
		}
	}
	return shadow;
}



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
	vec3 bumpedNormalW=norm;
	vec4 normalMapSample=vec4(1.0);
	
	
	
	if(enableNormalMap>0){
		//float4 normalMapSample = gTextureMaps[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
		normalMapSample = texture(sampler2D(textureMap[normalMapIndex],samp), inTexC);	
		//float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);
		bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, norm, inTangentW);
	}
	
	
	
	// Vector from point being lit to eye. 
    //float3 toEyeW = normalize(gEyePosW - pin.PosW);
	vec3 toEyeW = normalize(gEyePosW - inPosW);	
	float ambientAccess=1.0f;
	if(enableSsao>0){
		// Finish texture projection and sample SSAO map.
		//pin.SsaoPosH /= pin.SsaoPosH.w;
		//float ambientAccess = gSsaoMap.Sample(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;	
		vec4 ssaoPosH = inSsaoPosH/inSsaoPosH.w;
	
		ambientAccess = texture(ssaoMap,ssaoPosH.xy).r;
	}
	
	// Light terms.
    //float4 ambient = gAmbientLight*diffuseAlbedo;
	vec4 ambient = ambientAccess*gAmbientLight*diffuseAlbedo;
	
	// Only the first light casts a shadow.
    vec3 shadowFactor = vec3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(inShadowPosH);
	
	const float shininess = (1.0f - roughness);// * normalMapSample.a;
	Material mat = { diffuseAlbedo, fresnelR0, shininess };
    //float3 shadowFactor = 1.0f;
	//vec3 shadowFactor=vec3(1.0f);
    //float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
      //  bumpedNormalW, toEyeW, shadowFactor);
	vec4 directLight = ComputeLighting(mat,inPosW,bumpedNormalW,toEyeW,shadowFactor);
	  
	// float4 litColor = ambient + directLight;
    vec4 litColor = ambient + directLight;
	
		// Add in specular reflections.
	vec3 r = reflect(-toEyeW, bumpedNormalW);
	vec4 reflectionColor = texture(samplerCube(cubeMap,samp),r);
	vec3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
	litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
	
    // Common convention to take alpha from diffuse material.
    litColor.a = diffuseAlbedo.a;
	outFragColor = litColor;
}