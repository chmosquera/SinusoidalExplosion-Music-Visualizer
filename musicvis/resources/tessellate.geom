#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 4) out;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform float amplitude;
uniform float freq;
uniform float time;

in vec3 vcolor[];
out vec3 fcolor;

in vec3 geomPos[];
in vec3 geomNor[];
out vec3 fragPos;
out vec3 fragNor;

void main() {    

	// color
	fcolor = vcolor[0];

	fragPos = geomPos[0];
	fragNor = geomNor[0];
	
	// setup - normalize
	vec3 a = normalize(gl_in[0].gl_Position.xyz);
	vec3 b = normalize(gl_in[1].gl_Position.xyz);
	vec3 c = normalize(gl_in[2].gl_Position.xyz);
	vec3 d = normalize(b + c);

	// calculate normals of two triangles
	vec3 abd_nor = normalize(cross(b - a, d - a));
	vec3 adc_nor = normalize(cross(d - a, c - a));
	// not working
	
	
	// output
	gl_Position = P * V * M * vec4(b, 1.0);
	EmitVertex();
	gl_Position = P * V * M * vec4(d, 1.0);
	EmitVertex();
	gl_Position = P * V * M * vec4(a, 1.0);
	EmitVertex();
	gl_Position = P * V * M * vec4(c, 1.0);
	EmitVertex(); 
	EndPrimitive();
	
}  