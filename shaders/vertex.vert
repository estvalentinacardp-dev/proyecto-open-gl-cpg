// renderzado para cada vértice, pasando el color y la posición transformada al fragment shader
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in float aAngle;
out vec4 vColor;
uniform mat4 uMVP;
void main()
{
	vColor = aColor;
	gl_Position = uMVP * vec4(aPos, 1.0);
}
