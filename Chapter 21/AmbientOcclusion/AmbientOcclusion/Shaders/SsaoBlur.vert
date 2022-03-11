#version 450

layout (location=0) out vec4 outPosH;
layout (location=1) out vec2 outTexC;


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

layout (set=1,binding=0) uniform sampler2D normalMap;
layout (set=2,binding=0) uniform sampler2D depthMap;
layout (set=3,binding=0) uniform sampler2D inputMap;

 
 
 
const vec2 texCoords[6] =
{//NDC in vulkan start from top left corner, not bottom left.
	//vec2(0.0f, 0.0f),
	//vec2(0.0f, 1.0f),
	//vec2(1.0f,1.0f),
	//vec2(0.0f,0.0f),
	//vec2(1.0f,1.f),
	//vec2(1.0f,0.0f),
    vec2(0.0f, 1.0f),
    vec2(0.0f, 0.0f),
    vec2(1.0f, 0.0f),
    vec2(0.0f, 1.0f),
    vec2(1.0f, 0.0f),
    vec2(1.0f, 1.0f)
};


void main(){
	//outTexC = texCoords[gl_VertexIndex];//is gl_VertexIndex the same as gl_VertexID?
	outTexC = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	
	// Quad covering screen in NDC space.
    //outPosH = vec4(2.0f*outTexC.x - 1.0f, 1.0f - 2.0f*outTexC.y, 0.0f, 1.0f);
	vec4 posH = vec4(outTexC * 2.0f - 1.0f, 0.0f, 1.0f);
	gl_Position=posH;
	
}