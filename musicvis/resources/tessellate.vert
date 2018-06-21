#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertCol;
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 vertex_normal;
out vec3 vcolor;

void main()
{
	gl_Position = vec4(vertPos, 1.0);
	vcolor = vertPos;
}
