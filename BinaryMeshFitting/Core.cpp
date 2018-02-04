#include "PCH.h"
#include <sys/utime.h>
#include "Core.hpp"
#include "DebugScene.hpp"

#define MAX_FRAMESKIP 1
#define UPDATES_PER_SECOND 60.0
#define SKIP_TICKS 1.0 / UPDATES_PER_SECOND

static DebugScene* global_scene;

void Core_error_callback(int e, const char* s)
{
	printf("GL Error %d: %s\n", e, s);
}

int Core_init(struct RenderInput* out)
{
	if (!glfwInit())
		return -1;

	glfwSetErrorCallback(Core_error_callback);
	glfwWindowHint(GLFW_SAMPLES, 4);
	out->window = glfwCreateWindow(DEFAULT_RENDER_WIDTH, DEFAULT_RENDER_HEIGHT, "GLIsosurface", NULL, NULL);
	glfwSetWindowPos(out->window, 150, 100);
	if (!out->window)
	{
		glfwTerminate();
		return -1;
	}
	out->width = DEFAULT_RENDER_WIDTH;
	out->height = DEFAULT_RENDER_HEIGHT;

	glfwMakeContextCurrent(out->window);
	glfwSwapInterval(1);
	//glfwSetInputMode(out->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glewExperimental = GL_TRUE;
	glewInit();

	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	printf("Renderer: %s\n", renderer);
	printf("OpenGL version supported %s\n\n", version);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	return 0;
}

int Core_run(struct RenderInput* render)
{
	DebugScene scene(render);
	global_scene = &scene;
	glfwSetKeyCallback(render->window, key_callback);

	double last_time = glfwGetTime();
	double timer = last_time;
	double current_time = 0;
	double next_tick = last_time;
	int frame_counter = 0;
	int last_fps = 0;
	int update_counter = 0;
	int tupdate_counter = 0;

	while (!glfwWindowShouldClose(render->window))
	{
		current_time = glfwGetTime();
		render->delta = (float)(current_time - last_time);
		last_time = current_time;
		update_counter = 0;

		while (glfwGetTime() > next_tick && update_counter < MAX_FRAMESKIP)
		{
			scene.update(render);
			update_counter++;
			next_tick += SKIP_TICKS;
			tupdate_counter++;
		}

		scene.render(render);

		frame_counter++;

		if (glfwGetTime() - timer > 1.0)
		{
			timer++;
			last_fps = frame_counter;
			//printf("FPS: %i\t\tUpdates: %i\n", last_fps, tupdate_counter);
			frame_counter = 0;
			tupdate_counter = 0;
		}
	}

	return 0;
}

void Core_cleanup()
{
	glfwTerminate();
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	global_scene->key_callback(key, scancode, action, mods);
}
