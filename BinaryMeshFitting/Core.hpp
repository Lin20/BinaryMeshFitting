#pragma once

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdint.h>

#define DEFAULT_RENDER_WIDTH 1600
#define DEFAULT_RENDER_HEIGHT 900

struct RenderInput
{
	GLFWwindow* window;
	uint32_t width;
	uint32_t height;
	float delta;
};

int Core_init(struct RenderInput* out);
int Core_run(struct RenderInput* render);
void Core_cleanup();

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);