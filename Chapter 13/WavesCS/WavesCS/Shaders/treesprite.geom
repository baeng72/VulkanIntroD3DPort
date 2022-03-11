#version 450 
layout (points) in;
layout (triangle_strip,max_vertices=4) out;


layout(location=0) in vec3 inCenterW[];
layout(location=1) in vec2 inSizeW[];


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

void main(){
//
	// Compute the local coordinate system of the sprite relative to the world
	// space such that the billboard is aligned with the y-axis and faces the eye.
	//

	vec3 up = vec3(0.0f, 1.0f, 0.0f);
	vec3 look = gEyePosW - inCenterW[0];
	look.y = 0.0f; // y-axis aligned, so project to xz-plane
	look = normalize(look);
	vec3 right = cross(up, look);

	//
	// Compute triangle strip vertices (quad) in world space.
	//
	float halfWidth  = 0.5f*inSizeW[0].x;
	float halfHeight = 0.5f*inSizeW[0].y;
	
	vec4 v[4];
	v[0] = vec4(inCenterW[0] + halfWidth*right - halfHeight*up, 1.0f);
	v[1] = vec4(inCenterW[0] + halfWidth*right + halfHeight*up, 1.0f);
	v[2] = vec4(inCenterW[0] - halfWidth*right - halfHeight*up, 1.0f);
	v[3] = vec4(inCenterW[0] - halfWidth*right + halfHeight*up, 1.0f);

	//
	// Transform quad vertices to world space and output 
	// them as a triangle strip.
	//
	
	vec2 texC[4] = 
	{
		vec2(0.0f, 1.0f),
		vec2(0.0f, 0.0f),
		vec2(1.0f, 1.0f),
		vec2(1.0f, 0.0f)
	};
	
	
	//vec4 = gl
	for(int i=0;i<4;++i){
		gl_Layer=0;//one face?
		//gout.PosH     = mul(v[i], gViewProj);
		gl_Position = viewProj * v[i];
		
		//gout.PosW     = v[i].xyz;
		outPosW = v[i].xyz;
		//gout.NormalW  = look;
		outNormalW = look;
		//gout.TexC     = texC[i];
		outTexC = texC[i];
		//gout.PrimID   = primID;
		EmitVertex();
	}
	EndPrimitive();
    
}