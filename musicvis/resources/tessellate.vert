#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertCol;
layout(location = 2) in vec3 vertNor;
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 vertex_normal;
out vec3 vcolor;


out vec3 geomPos;
out vec3 geomNor;

void main()
{
	gl_Position = vec4(vertPos, 1.0);
	vcolor = vertPos;

	geomPos = vec4(V * M * vec4(vertPos, 1.0)).xyz;
    geomNor = vec4(V * M * vec4(vertNor, 1.0)).xyz;
}
