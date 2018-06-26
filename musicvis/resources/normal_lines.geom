#version 330 core
layout (triangles) in;
layout (line_strip, max_vertices = 8) out;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform float amplitude;
uniform float freq;
uniform float time;

in vec3 vcolor[];
out vec3 fcolor;
in vec3 vnormal[];
out vec3 fnormal;
in vec3 vpos[];
out vec3 fpos;

void main() {    


	// pass through
	fcolor = vcolor[0];
	fnormal = vnormal[0];
	fpos = vpos[0];
	
	// setup - normalize
	vec3 a = normalize(gl_in[0].gl_Position.xyz);
	vec3 b = normalize(gl_in[1].gl_Position.xyz);
	vec3 c = normalize(gl_in[2].gl_Position.xyz);
	vec3 d = normalize(b + c);

	vec4 center = (gl_in[0].gl_Position + gl_in[1].gl_Position + gl_in[2].gl_Position)/3.0;
	vec3 face_normal = normalize(cross(b - a, c - a));
	
	
	for (int i = 0; i < gl_in.length(); i++) {

		float normLength = 2.0 * amplitude;

		/*// draws normals
		gl_Position = center;
		EmitVertex();
		gl_Position = (center + vec4(face_normal * normLength, 0.0));
		EmitVertex();
		*/
		
		// draws stars
		gl_Position = center;
		EmitVertex();
		gl_Position = center + vec4(1.0, 1.0, 0.0, 0.0);
		EmitVertex();
		
	
		
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
		gl_Position = center * normLength;		// creates soccer ball
		EmitVertex();
		

		/*
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
		float circle = sqrt(pow(gl_in[i].gl_Position.x, 2.0) + pow(gl_in[i].gl_Position.y, 2.0)  + pow(gl_in[i].gl_Position.z, 2.0));
		gl_Position = gl_in[i].gl_Position * circle;// vec4(0.0, 0.0, circle, 1.0);
		EmitVertex();
		*/
		

	}

	EndPrimitive();
	
}  