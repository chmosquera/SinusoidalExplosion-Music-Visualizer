#version 330 core
out vec4 color;
in vec3 fcolor;

in vec3 fragPos;
in vec3 fragNor;

uniform mat4 V;

void main()
{
	color = vec4(fcolor, 1.0);	

	
	
	//color = vec4(fragNor, 1.0);
	vec3 lightPos = vec3(100,-100,100);
	vec3 lightDir = normalize(lightPos - fragPos);
    vec3 viewDir = normalize(-fragPos);
    vec3 normal = normalize(fragNor);
    
    // Ambient calculation
    vec3 ambientColor = vec3(0.5, 0.2, 1.0);
    
    // Diffuse calculation
//    float diffuseFactor = max(dot(lightDir, normal), 0); // Clamp to prevent color from reaching back side
    float diffuseFactor = (dot(lightDir, normal) + 1) / 2; // Normalize so the color bleeds onto the back side
    vec3 diffuseColor = vec3(0.2, 0.1, 0.5) * diffuseFactor;
    
    // Specular calculation
    float specularStrength = 0.5f;
    vec3 reflectDir = reflect(-lightDir, normal);
    float shininess = 3.0f;
    float specularFactor = pow(max(dot(viewDir, reflectDir), 0), shininess);
    vec3 specularColor = vec3(1) * specularFactor * specularStrength;

	color.rgb = diffuseColor;
    //color = vec4(ambientColor + diffuseColor + specularColor, 1); 
}
