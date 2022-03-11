#version 450
//#define GL_EXT_samplerless_texture_functions


layout(location=0) in vec2 inTexC;
layout(location=0) out vec4 outFragColor;


layout (set=5,binding=0) uniform sampler2D shadowMap;



void main(){
	outFragColor =  vec4(texture(shadowMap,inTexC).rrr,1.0f);
}