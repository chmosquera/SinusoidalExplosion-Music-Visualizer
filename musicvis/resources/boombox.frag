#version 330 core
out vec4 color;
in vec3 fcolor;
uniform float freq;
uniform float time;

void main()
{

	color = vec4(fcolor, 1.0);
	color.rgb = vec3(1.0, 0.5, 0.5);
	vec3 color1 = vec3(1.0, 0.0, 0.0);
	vec3 color2 = vec3(0.0, 1.0, 0.0);
	float t = ((sin( time )*sin(freq)) + 1.0)/2.0;	


	color.rgb = mix(color1, color2, t); 
	color.a = 0.9;

}
