#include "PCH.h"

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <stdio.h>

#include "Core.hpp"

#include <Vc/Vc>

#if defined(_DEBUG) && defined(_WIN32)
#define DWIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

int main(void)
{
	RenderInput render_input;

	if (Core_init(&render_input))
	{
		glfwTerminate();
		return -1;
	}

	Core_run(&render_input);
	Core_cleanup();

#ifdef DWIN32
	//Print out any memory leaks
	_CrtDumpMemoryLeaks();
#endif

	return 0;
}
