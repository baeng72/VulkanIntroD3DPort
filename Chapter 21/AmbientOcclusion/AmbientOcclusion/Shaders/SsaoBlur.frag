#version 450

layout (location=0) in vec4 inPosH;
layout (location=1) in vec2 inTexC;

layout (location=0) out vec4 outFragColor;


layout (set=0,binding=0) uniform cbSsao
{
    mat4 Proj;
    mat4 InvProj;
    mat4 ProjTex;
	vec4 offsetVectors[14];

    // For SsaoBlur.hlsl
    vec4 blurWeights[3];

    vec2 invRenderTargetSize;

    // Coordinates given in view space.
    float    occlusionRadius;
    float    occlusionFadeStart;
    float    occlusionFadeEnd;
    float    surfaceEpsilon;
};

const int sampleCount = 14;

layout (set=1,binding=0) uniform sampler2D normalMap;
layout (set=2,binding=0) uniform sampler2D depthMap;
layout (set=3,binding=0) uniform sampler2D inputMap;

layout (constant_id=0) const int horizontalBlur=1;

const int blurRadius = 5;
 


float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = Proj[3][2] / (z_ndc - Proj[2][2]);
    return viewZ;
}



void main(){
	// unpack into float array.
    float weights[12] =
    {
        blurWeights[0].x, blurWeights[0].y, blurWeights[0].z, blurWeights[0].w,
        blurWeights[1].x, blurWeights[1].y, blurWeights[1].z, blurWeights[1].w,
        blurWeights[2].x, blurWeights[2].y, blurWeights[2].z, blurWeights[2].w,
    };
	vec2 texOffset;
	if(horizontalBlur>0)
	{
		texOffset = vec2(invRenderTargetSize.x, 0.0f);
	}
	else
	{
		texOffset = vec2(0.0f, invRenderTargetSize.y);
	}
	
	// The center value always contributes to the sum.
	vec4 color      = weights[blurRadius] * texture(inputMap,inTexC);
	float totalWeight = weights[blurRadius];
	
	vec3 centerNormal = texture(normalMap, inTexC).xyz;
    float  centerDepth = NdcDepthToViewDepth(
        texture(depthMap, inTexC).r);
		
	for(int i = -blurRadius; i <=blurRadius; ++i)
	{
		// We already added in the center weight.
		if( i == 0 )
			continue;

		vec2 tex = inTexC + i*texOffset;

		vec3 neighborNormal = texture(normalMap, tex).xyz;
        float  neighborDepth  = NdcDepthToViewDepth(
            texture(depthMap, tex).r);

		//
		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
		//
	
		if( dot(neighborNormal, centerNormal) >= 0.8f &&
		    abs(neighborDepth - centerDepth) <= 0.2f )
		{
            float weight = weights[i + blurRadius];

			// Add neighbor pixel to blur.
			color += weight*texture(inputMap,tex);
		
			totalWeight += weight;
		}
	}

	// Compensate for discarded samples by making total weights sum to 1.
    outFragColor= color / totalWeight;

	
}