#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inTexC;
layout(location=3) in vec3 inTangentU;
layout(location=0) out vec2 outTexC;


void main(){
	gl_Position=vec4(inPos.x,-inPos.y,inPos.z,1.0);
	
    outTexC = inTexC;
	
}