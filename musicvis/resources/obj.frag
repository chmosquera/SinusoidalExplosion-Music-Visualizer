#version 330 core
out vec4 color;
in vec3 fcolor;

uniform float amplitude;
uniform float freq;

void main()
{
	color.xyz = fcolor;	// gets darker sometimes
	//color.xyz += vec3(1.0) * max(freq, 0.8);
	
	//float f = freq - int(freq);
	//color.r *= f;
	//color.b *= (1.0 - f);
	color.a = 1.0;
	

}
