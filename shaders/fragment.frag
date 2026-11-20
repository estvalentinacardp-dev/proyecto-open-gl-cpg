// hace el color de cada fragmento

#version 330 core
// Interpolación de color desde el vértice
in vec4 vColor;
out vec4 FragColor;
void main()
{
	FragColor = vColor;
}

