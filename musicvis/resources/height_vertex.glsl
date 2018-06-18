#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
out vec3 vertex_pos;
out vec2 vertex_tex;
uniform sampler2D tex;
uniform sampler2D tex2;
uniform vec3 camoff;

void main()
{

	vec2 texcoords=vertTex;
	float t=1./1000.;
	texcoords -= vec2(camoff.x,camoff.z)*t;

	float baseheight = texture(tex2, texcoords*1).r;

	float audioheight = texture(tex, vec2(texcoords.y,texcoords.x)).r;
	//float audioheight_scale = pow(2, audioheight);
	//height *= 50.0-5.0;


	vec4 tpos =  vec4(vertPos, 1.0);
	tpos.z -=camoff.z;
	tpos.x -=camoff.x;

	tpos =  M * tpos;

	vec2 blackholepos = vec2(10,-1000);
	float dist = length(tpos.xz - blackholepos);
	float blackholedepth = log(dist-105)*100-700;
	
	tpos.y += baseheight*50*(1.-audioheight)+blackholedepth;
	vertex_pos = tpos.xyz;



	gl_Position = tpos;

	vertex_tex = vertTex;
}
