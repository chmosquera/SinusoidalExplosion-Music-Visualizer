#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertCol;
layout(location = 2) in vec3 vertNor;
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 vnormal;
out vec3 vcolor;
out vec3 vpos;

void main()
{
	gl_Position = P * V * M * vec4(vertPos, 1.0);
	vcolor = vertPos;
	vnormal = vertNor;
	vpos = vec3(P * V * M * vec4(vertPos, 1.0));
}
