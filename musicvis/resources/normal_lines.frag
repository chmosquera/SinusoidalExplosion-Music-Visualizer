#version 330 core
out vec4 color;
in vec3 fcolor;
in vec3 fnormal;
in vec3 fpos;


uniform mat4 V;
uniform float angle;
uniform float freq;
uniform float amplitude;

void main()
{
	color = vec4(fcolor, 1.0);	
	color.rgb += vec3(150);

	color.a = abs(sin(2.0 * angle));
	color.a = max(0.2, abs(freq/2.0) * 10.0);

//	if (fpos.x > 0.0 && fpos.y >0.0 ) color.a = 1.0;
}
