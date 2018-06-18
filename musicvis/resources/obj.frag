#version 330 core
out vec4 color;
in vec3 fcolor;

uniform float explode;
uniform float freq;

void main()
{
	color.xyz = fcolor;
	
	float f = freq - int(freq);
	color.r *= f;
	color.b *= (1.0 - f);

	float white = explode/2.0;
	color.xyz *= white;
	
	color.a = 1;

}
