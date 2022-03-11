#version 450
layout(location=0) in vec3 NormalW;
layout(location=1) in vec3 PosW;
layout(location=2) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

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
	uint     MatPad0;
	uint     MatPad1;
	uint     MatPad2;
};

layout (set=2, binding=0) readonly buffer MaterialBuffer{
	MaterialData materials[];
}materialData;


layout (set=3,binding=0) uniform sampler samp;
layout (set=3,binding=1) uniform texture2D textureMap[4];
layout (set=3,binding=0) uniform samplerCube cubeMap;

#define NUM_DIR_LIGHTS 3
#define NUM_POINT_LIGHTS 0
#define NUM_SPOT_LIGHTS 0

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

vec3 computeDirLights(Material mat, vec3 normal, vec3 toEye,vec3 shadowFactor){
	vec3 result=vec3(0.0);
	result += shadowFactor[0] * ComputeDirectionalLight(gLights[0], mat, normal, toEye);
	result += shadowFactor[1] * ComputeDirectionalLight(gLights[1], mat, normal, toEye);
	result += shadowFactor[2] * ComputeDirectionalLight(gLights[2], mat, normal, toEye);
	return result;
}

vec4 ComputeLighting( Material mat,
                       vec3 pos, vec3 normal, vec3 toEye,
                       vec3 shadowFactor)
{
    vec3 result = vec3(0.0);

   // int i = 0;

	result = computeDirLights(mat,normal,toEye,shadowFactor);
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




void main(){
	MaterialData matData = materialData.materials[gMaterialIndex];
	vec4 diffuseAlbedo = matData.DiffuseAlbedo;
	vec3 fresnelR0 = matData.FresnelR0;
	float roughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;
	
	diffuseAlbedo *= texture(sampler2D(textureMap[diffuseTexIndex],samp),TexCoord);
	vec3 norm = normalize(NormalW);
	// Vector from point being lit to eye.
	vec3 toEyeW = gEyePosW - PosW;	
	float distToEye = length(toEyeW);
    toEyeW /=distToEye;//normalize(gEyePosW - PosW);

	// Indirect lighting.
    vec4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    vec3 shadowFactor = vec3(1.0);
    vec4 directLight = ComputeLighting( mat, PosW, 
        norm, toEyeW, shadowFactor);

    vec4 litColor = ambient + directLight;
	
	//fog
	//float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	float fogAmount = clamp((distToEye-gFogStart)/gFogRange,0,1);
	litColor = mix(litColor, gFogColor, fogAmount);
	
	// Add in specular reflections.
	vec3 r = reflect(-toEyeW, norm);
	vec4 reflectionColor = texture(cubeMap, r);
	vec3 fresnelFactor = SchlickFresnel(fresnelR0, norm, r);
	litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;

    // Common convention to take alpha from diffuse material.
    litColor.a = diffuseAlbedo.a;
	FragColor = litColor;
}