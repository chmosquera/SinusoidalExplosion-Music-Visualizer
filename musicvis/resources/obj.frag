#version 330 core
out vec4 color;
in vec3 fcolor;

uniform float amplitude;
uniform float freq;

void main()
{
	color.xyz = fcolor * (1.0 - abs(freq));	// gets darker sometimes
	
	//float f = freq - int(freq);
	//color.r *= f;
	//color.b *= (1.0 - f);
	color.a = 1.0;
	

}
