#version 450

layout(location=0) in vec3 inPosL;
layout(location=1) in vec3 inNormalL;
layout(location=2) in vec2 inTexC;
layout(location=3) in vec3 inTangentL;
layout(location=4) in vec3 inBoneWeights;
layout(location=5) in ivec4 inBoneIndices;
layout(location=0) out vec3 outPosW;
layout(location=1) out vec4 outShadowPosH;
layout(location=2) out vec4 outSsaoPosH;
layout(location=3) out vec3 outNormalW;
layout(location=4) out vec3 outTangentW;
layout(location=5) out vec2 outTexC;

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
	
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = inBoneWeights.x;
    weights[1] = inBoneWeights.y;
    weights[2] = inBoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
	
	vec3 posL = vec3(0.0f, 0.0f, 0.0f);
    vec3 normalL = vec3(0.0f, 0.0f, 0.0f);
    vec3 tangentL = vec3(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < 4; ++i)
    {
        // Assume no nonuniform scaling when transforming normals, so 
        // that we do not have to use the inverse-transpose.

        //posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
		posL += weights[i] * (boneTransforms[inBoneIndices[i]]*vec4(inPosL,1.0f)).xyz;
        //normalL += weights[i] * mul(vin.NormalL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
		normalL += weights[i] * (mat3(boneTransforms[inBoneIndices[i]])*inNormalL);
        //tangentL += weights[i] * mul(vin.TangentL.xyz, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
		tangentL += weights[i]*(mat3(boneTransforms[inBoneIndices[i]])*inTangentL.xyz);
    }

    //vin.PosL = posL;
    //vin.NormalL = normalL;
    //vin.TangentL.xyz = tangentL;
	
	// Transform to world space.
    //float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    //vout.PosW = posW.xyz;
	vec4 posW = world * vec4(posL,1.0);
	outPosW = posW.xyz;
	
	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    //vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
	outNormalW = mat3(world)*normalL;
		
	//vout.TangentW = mul(vin.TangentL, (float3x3)gWorld);
	outTangentW = mat3(world)*tangentL;

	// Transform to homogeneous clip space.
    //vout.PosH = mul(posW, gViewProj);
	gl_Position = viewProj * posW;
	
	 // Generate projective tex-coords to project SSAO map onto scene.
    //vout.SsaoPosH = mul(posW, gViewProjTex);
	outSsaoPosH = viewProjTex * vec4(outPosW,1.0);
	
	// Output vertex attributes for interpolation across triangle.
    vec4 texC = texTransform  *vec4(inTexC, 0.0f, 1.0f);	
    outTexC = (matData.MatTransform*texC).xy;
	
	// Generate projective tex-coords to project shadow map onto scene.
	//vout.ShadowPosH = mul(posW, gShadowTransform);
    outShadowPosH = shadowTransform*vec4(outPosW,1.0);
}