#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
out vec3 vertex_pos;
out vec2 vertex_tex;
uniform sampler2D tex;
uniform sampler2D tex2;
uniform vec3 camoff;

in vec3 vcolor[];
out vec3 fcolor;

const float PI = 3.1415926;

void main() {    

	// color
	fcolor = vcolor[0];

	// find face normal
	// Calculate two vectors in the plane of the input triangle
	vec3 ab = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 ac = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 normal = normalize(cross(ac, ab));

	// DRAW whole obj
	for (int i = 0; i < gl_in.length(); i++){

		float t = clamp(transClock, 0, 1);
		t = t * t * (3 - 2 * t);			// smooth step

		 // MAKES THE GEOMETRY OF THE OBJECT DANCE....
		float len = sqrt(gl_in[i].gl_Position.x * gl_in[i].gl_Position.x 
					+ gl_in[i].gl_Position.z * gl_in[i].gl_Position.z);

		float sinwave = abs(sin( time + (10.0*freq)  + len));					// play with * vs. +	
		//sinwave = freq * len;
		float fxn = pow(sinwave, 1.0);		// what does changing the power do? has some type of rhythm
											// 1 -> 100 :: slow -> fast
		
		// draw
		float scale = 10.0;		// not sure what the view is so i have to scale
		vec4 B = vec4(pointB * scale, 0.0, 0.0);
		vec4 A = vec4(pointA * scale, 0.0, 0.0);

		gl_Position = (gl_in[i].gl_Position + vec4(explode * normal * fxn, 0.0))
					+ (A * (1 - t))
					+ (B * t);

		EmitVertex();
	}
	
	EndPrimitive();
}  