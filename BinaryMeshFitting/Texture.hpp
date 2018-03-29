#pragma once
#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>

class Texture
{
public:
	GLuint id;

	bool initialized;
	int width;
	int height;
	int length;
	int bpp;

	Texture();
	~Texture();

	bool load_from_file(std::string filename);
	void init(unsigned char* src);
	void init(int width, int height, int length, unsigned char* src);
};
