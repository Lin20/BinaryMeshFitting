#include "PCH.h"
#include "Texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::Texture()
{
	id = 0;
	initialized = false;
}

Texture::~Texture()
{
	if (initialized)
	{
		glDeleteTextures(1, &id);
		initialized = false;
	}
}

bool Texture::load_from_file(std::string filename)
{
	unsigned char* rgb = stbi_load(filename.c_str(), &width, &height, &bpp, 3);
	if (!rgb)
		return false;

	init(rgb);

	stbi_image_free(rgb);

	return true;
}

void Texture::init(unsigned char* src)
{
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)width, (GLsizei)height, 0, GL_RGB, GL_UNSIGNED_BYTE, src);

	glBindTexture(GL_TEXTURE_2D, 0);

	initialized = true;
}

void Texture::init(int width, int height, int length, unsigned char* src)
{
	this->width = width;
	this->height = height;
	this->length = length;

	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_3D, id);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, (GLsizei)width, (GLsizei)height, (GLsizei)length, 0, GL_RGB, GL_UNSIGNED_BYTE, src);

	glBindTexture(GL_TEXTURE_3D, 0);

	initialized = true;
}
