#version 450


layout (location=0) in vec3 inPosV;
layout (location=1) in vec2 inTexC;

layout (location=0) out float outFragColor;


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
layout (set=3,binding=0) uniform sampler2D randomVecMap;

 

// Determines how much the sample point q occludes the point p as a function
// of distZ.
float OcclusionFunction(float distZ)
{
	//
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.
	//
	// We use the following function to determine the occlusion.  
	// 
	//
	//       1.0     -------------\
	//               |           |  \
	//               |           |    \
	//               |           |      \ 
	//               |           |        \
	//               |           |          \
	//               |           |            \
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps          z0            z1        
	//
	
	float occlusion = 0.0f;
	if(distZ > surfaceEpsilon)
	{
		float fadeLength = occlusionFadeEnd - occlusionFadeStart;
		
		// Linearly decrease occlusion from 1 to 0 as distZ goes 
		// from gOcclusionFadeStart to gOcclusionFadeEnd.	
		occlusion = clamp((occlusionFadeEnd-distZ)/fadeLength,0.0,1.0);// saturate( (occlusionFadeEnd-distZ)/fadeLength );
	}
	
	return occlusion;	
}

float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = Proj[3][2] / (z_ndc - Proj[2][2]);
    return viewZ;
}



void main(){
	// p -- the point we are computing the ambient occlusion for.
	// n -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.
	
	// Get viewspace normal and z-coord of this pixel.  
    vec3 n = normalize(texture(normalMap,inTexC)).xyz;//.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz);
    float pz = texture(depthMap,inTexC).r;//gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r;
    pz = NdcDepthToViewDepth(pz);
	
	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	vec3 p = (pz/inPosV.z)*inPosV;
	
	// Extract random vector and map from [0,1] --> [-1, +1].
	//vec3 randVec = 2.0f*texture(randomVecMap, 4.0f*inTexC).rgb - 1.0f;
	vec2 texC = 4.0f*inTexC;
	vec3 samp = texture(randomVecMap,texC).rgb;
	vec3 randVec = 2.0f*samp-1.0f;
	
	
	
	float occlusionSum = 0.0f;
	
	// Sample neighboring points about p in the hemisphere oriented by n.
	for(int i = 0; i < sampleCount; ++i)
	{
		// Are offset vectors are fixed and uniformly distributed (so that our offset vectors
		// do not clump in the same direction).  If we reflect them about a random vector
		// then we get a random uniform distribution of offset vectors.
		vec3 offset = reflect(offsetVectors[i].xyz, randVec);
	
		// Flip offset vector if it is behind the plane defined by (p, n).
		float flip = sign( dot(offset, n) );
		
		// Sample a point near p within the occlusion radius.
		vec3 q = p + flip * occlusionRadius * offset;
		
		// Project q and generate projective tex-coords.  
		vec4 projQ = ProjTex * vec4(q,1.0f);//mul(float4(q, 1.0f), ProjTex);
		projQ /= projQ.w;

		// Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might
		// occupy empty space).  To find the nearest depth we look it up in the depthmap.

		float rz = texture(depthMap,projQ.xy).r;// gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r;
        rz = NdcDepthToViewDepth(rz);

		// Reconstruct full view space position r = (rx,ry,rz).  We know r
		// lies on the ray of q, so there exists a t such that r = t*q.
		// r.z = t*q.z ==> t = r.z / q.z

		vec3 r = (rz / q.z) * q;
		
		//
		// Test whether r occludes p.
		//   * The product dot(n, normalize(r - p)) measures how much in front
		//     of the plane(p,n) the occluder point r is.  The more in front it is, the
		//     more occlusion weight we give it.  This also prevents self shadowing where 
		//     a point r on an angled plane (p,n) could give a false occlusion since they
		//     have different depth values with respect to the eye.
		//   * The weight of the occlusion is scaled based on how far the occluder is from
		//     the point we are computing the occlusion of.  If the occluder r is far away
		//     from p, then it does not occlude it.
		// 
		
		float distZ = p.z - r.z;
		float dp = max(dot(n, normalize(r - p)), 0.0f);

        float occlusion = dp*OcclusionFunction(distZ);

		occlusionSum += occlusion;
	}
	
	occlusionSum /= sampleCount;
	
	float access = 1.0f - occlusionSum;

	// Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
	outFragColor =  clamp(pow(access, 6.0f),0.0,1.0f);
}