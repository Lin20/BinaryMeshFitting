#include "PCH.h"
#include "DebugScene.hpp"
#include "Core.hpp"
#include "DefaultOptions.h"
#include "ImplicitSampler.hpp"
#include "NoiseSampler.hpp"
#include "MeshProcessor.hpp"
#include "CubicChunk.hpp"
#include <time.h>
#include <iostream>
#include <omp.h>

#include "GUI/imgui.h"
#include "GUI/imgui_impl_glfw_gl3.h"

using namespace glm;

#define SHADER_ERROR_CHECK(SHADER, TEXT) \
glGetShaderiv(SHADER, GL_COMPILE_STATUS, &success); \
if (success == GL_FALSE) \
{ \
	std::cout << "Failed to compile " << TEXT << "." << std::endl; \
	glGetShaderiv(SHADER, GL_INFO_LOG_LENGTH, &log_size); \
	char* error_msg = new char[log_size]; \
	glGetShaderInfoLog(SHADER, log_size, &log_size, error_msg); \
	printf(error_msg); \
	printf("\n"); \
	delete[] error_msg; \
}

#define LINKER_ERROR_CHECK(SHADER, TEXT) \
glGetProgramiv(SHADER, GL_LINK_STATUS, &success); \
if (success == GL_FALSE) \
{ \
	std::cout << "Failed to link " << TEXT << "." << std::endl; \
	glGetProgramiv(SHADER, GL_INFO_LOG_LENGTH, &log_size); \
	char* error_msg = new char[log_size]; \
	glGetProgramInfoLog(SHADER, log_size, &log_size, error_msg); \
	printf(error_msg); \
	printf("\n"); \
	delete[] error_msg; \
}

DebugScene::DebugScene(RenderInput* render_input)
{
	this->last_space = 0;
	this->outline_visible = false;
	this->smooth_shading = SMOOTH_NORMALS;
	this->fillmode = DEFAULT_FILL_MODE;
	this->quads = QUADS;
	this->flat_quads = FLAT_QUADS;
	this->cull = true;
	this->gui_visible = true;
	this->auto_update = true;
	this->line_width = 1.0f;
	this->specular_power = SPECULAR_POWER;

	this->fill_color[0] = 0.85f;
	this->fill_color[1] = 0.85f;
	this->fill_color[2] = 0.85f;
	this->fill_color[3] = 1.0f;

	this->line_color[0] = 0.25f;
	this->line_color[1] = 0.25f;
	this->line_color[2] = 0.25f;
	this->line_color[3] = 1.0f;

	this->clear_color[0] = 0.0f;
	this->clear_color[1] = 0.5f;
	this->clear_color[2] = 1.0f;
	this->clear_color[3] = 1.0f;

	float dx[] = { 0, 1, 0, 1, 0, 1, 0, 1 };
	float dy[] = { 0, 0, 1, 1, 0, 0, 1, 1 };
	float dz[] = { 0, 0, 0, 0, 1, 1, 1, 1 };

	const char* vertex_shader =
		"#version 400 core\n"
		"attribute vec3 vertex_position;\n"
		"attribute vec3 vertex_normal;\n"
		"attribute vec3 vertex_color;\n"
		"uniform mat4 projection;\n"
		"uniform mat4 view;\n"
		"uniform vec3 mul_color;\n"
		"uniform float smooth_shading;\n"
		"uniform float specular_power;\n"
		"out vec3 f_normal;\n"
		"out vec3 f_color;\n"
		"out vec3 f_mul_color;\n"
		"out vec3 f_ec_pos;\n"
		"out float f_smooth_shading;\n"
		"out float f_specular_power;\n"
		"out float log_z;"
		"void main() {\n"
		"  f_normal = normalize(vertex_normal);\n"
		"  f_color = vertex_color;\n"
		"  f_mul_color = mul_color;\n"
		"  f_smooth_shading = smooth_shading;\n"
		"  f_specular_power = specular_power;\n"
		"  f_ec_pos = vertex_position;\n"
		"  const float near = 0.00001;"
		"  const float far = 10000.0;"
		"  const float C = 0.001;"
		"  gl_Position = projection * view * vec4(vertex_position, 1);\n"
		"  const float FC = 1.0f / log(far * C + 1.0);"
		"  log_z = log(gl_Position.w * C + 1.0) * FC;"
		"  gl_Position.z = (2.0 * log_z - 1.0) * gl_Position.w;"
		"}";

	const char* fragment_shader =
		"#version 400 core\n"
		"in vec3 f_normal;\n"
		"in vec3 f_color;\n"
		"in vec3 f_mul_color;\n"
		"in vec3 f_ec_pos;\n"
		"in float f_smooth_shading;\n"
		"in float f_specular_power;\n"
		"in float log_z;\n"
		"out vec4 frag_color;\n"
		"void main() {"
		"  vec3 normal;\n"
		"  if (f_smooth_shading != 0.0)"
		"    normal = f_normal;\n"
		"  else"
		"    normal = normalize(cross(dFdx(f_ec_pos), dFdy(f_ec_pos)));\n"
		"  float d = dot(normalize(-vec3(0.1, -1.0, 0.5)), normal);\n"
		"  float m = mix(0.2, 1.0, d * 0.5 + 0.5);\n"
		"  float s = (f_specular_power > 0.0 ? pow(max(0.0, d), f_specular_power) : 0.0);\n"
		"  vec3 color = vec3(0.3, 0.3, 0.5);\n"
		"  vec3 color2 = vec3(0.1, 0.1, 0.25);\n"
		"  vec3 result = f_color * m + f_color * s;\n"
		"  frag_color = vec4(result, 1.0);\n"
		"  gl_FragDepth = log_z;"
		"}";

	const char* outline_vs =
		"#version 400 core\n"
		"attribute vec3 vertex_position;\n"
		"uniform mat4 projection;\n"
		"uniform mat4 view;\n"
		"uniform vec3 mul_color;\n"
		"out vec3 f_mul_color;\n"
		"out float log_z;"
		"void main() {\n"
		"  f_mul_color = mul_color;\n"
		"  gl_Position = projection * view * vec4(vertex_position, 1);\n"
		"  const float near = 0.00001;"
		"  const float far = 10000.0;"
		"  const float C = 0.001;"
		"  const float FC = 1.0 / log(far * C + 1.0);"
		"  log_z = log(gl_Position.w * C + 1.0) * FC;"
		"  gl_Position.z = (2.0 * log_z - 1.0) * gl_Position.w;"
		"}";

	const char* outline_fs =
		"#version 400 core\n"
		"in vec3 f_mul_color;\n"
		"in float log_z;\n"
		"out vec4 frag_color;\n"
		"void main() {\n"
		"  frag_color = vec4(f_mul_color, 1.0);\n"
		"  gl_FragDepth = log_z - 0.00001;"
		"}";

	GLint success;
	GLint log_size = 0;
	this->vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(this->vertex_shader, 1, &vertex_shader, NULL);
	glCompileShader(this->vertex_shader);
	SHADER_ERROR_CHECK(this->vertex_shader, "regular vertex shader");

	this->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(this->fragment_shader, 1, &fragment_shader, NULL);
	glCompileShader(this->fragment_shader);
	SHADER_ERROR_CHECK(this->fragment_shader, "regular fragment shader");

	this->shader_program = glCreateProgram();
	glAttachShader(this->shader_program, this->fragment_shader);
	glAttachShader(this->shader_program, this->vertex_shader);

	glBindAttribLocation(this->shader_program, 0, "vertex_position");
	glBindAttribLocation(this->shader_program, 1, "vertex_normal");

	glLinkProgram(this->shader_program);
	LINKER_ERROR_CHECK(this->shader_program, "regular shader");
	this->shader_projection = glGetUniformLocation(this->shader_program, "projection");
	this->shader_view = glGetUniformLocation(this->shader_program, "view");
	this->shader_mul_clr = glGetUniformLocation(this->shader_program, "mul_color");
	this->shader_eye_pos = glGetUniformLocation(this->shader_program, "eye_pos");
	this->shader_smooth_shading = glGetUniformLocation(this->shader_program, "smooth_shading");
	this->shader_specular_power = glGetUniformLocation(this->shader_program, "specular_power");

	this->outline_vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(this->outline_vs, 1, &outline_vs, NULL);
	glCompileShader(this->outline_vs);
	SHADER_ERROR_CHECK(this->outline_vs, "outline vertex shader");
	this->outline_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(this->outline_fs, 1, &outline_fs, NULL);
	glCompileShader(this->outline_fs);
	SHADER_ERROR_CHECK(this->outline_fs, "outline fragment shader");

	this->outline_sp = glCreateProgram();
	glAttachShader(this->outline_sp, this->outline_fs);
	glAttachShader(this->outline_sp, this->outline_vs);

	glBindAttribLocation(this->outline_sp, 0, "vertex_position");

	glLinkProgram(this->outline_sp);
	LINKER_ERROR_CHECK(this->outline_sp, "outline shader");
	this->outline_shader_projection = glGetUniformLocation(this->outline_sp, "projection");
	this->outline_shader_view = glGetUniformLocation(this->outline_sp, "view");
	this->outline_shader_mul_clr = glGetUniformLocation(this->outline_sp, "mul_color");

	this->camera.init(render_input->width, render_input->height, render_input);
	this->camera.set_shader(this->shader_projection, this->shader_view);

	printf("Initializing imgui...");
	ImGui_ImplGlfwGL3_Init(render_input->window, true);
	ImGui::StyleColorsLight();
	ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.85f;
	printf("Done.\n");

	dual_chunk = 0;
	binary_chunk = 0;

	init_world();
	//init_single_chunk();
	//init_binary_chunk();
}

DebugScene::~DebugScene()
{
	auto_update = false;
	update_thread.join();

	delete dual_chunk;
	delete binary_chunk;
	ImGui_ImplGlfwGL3_Shutdown();
}

void DebugScene::init_single_chunk()
{
	using namespace std;
	const int test_size = 256;
	//Sampler sampler = ImplicitFunctions::create_sampler(ImplicitFunctions::sphere);
	//sampler.block = ImplicitFunctions::torus_z_block;
	Sampler sampler;
	NoiseSamplers::create_sampler_noise3d(&sampler);
	cout << "Noise SIMD instruction set: " << get_simd_text() << endl;
	sampler.world_size = 256;

	SmartContainer<DualVertex> v_out(0);
	SmartContainer<uint32_t> i_out(262144);

	dual_chunk = new CubicChunk(vec3(-test_size, -test_size, -test_size) * 0.5f, (float)test_size, 0, sampler, QUADS);
	double extract_time = dual_chunk->extract(v_out, i_out, false);

	clock_t start_clock = clock();
	cout << "Processing...";

	int iters = 15;
	if (iters > 0 || !QUADS)
	{
		Processing::MeshProcessor<4> mp(true, SMOOTH_NORMALS);
		mp.init(v_out, i_out, sampler);
		if (iters > 0)
			mp.collapse_bad_quads();
		if (!QUADS)
		{
			v_out.count = 0;
			i_out.count = 0;
			mp.flush_to_tris(v_out, i_out);
			Processing::MeshProcessor<3> nmp = Processing::MeshProcessor<3>(true, SMOOTH_NORMALS);
			if (iters > 0)
			{
				nmp.init(v_out, i_out, sampler);
				nmp.optimize_dual_grid(iters);
				nmp.optimize_primal_grid(false, false);
				v_out.count = 0;
				i_out.count = 0;
				nmp.flush(v_out, i_out);
			}
		}
		else
		{
			mp.optimize_dual_grid(iters);
			mp.optimize_primal_grid(false, false);
			v_out.count = 0;
			i_out.count = 0;
			mp.flush(v_out, i_out);
		}
	}

	double elapsed = clock() - start_clock;
	cout << "done (" << (int)(elapsed / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
	cout << endl << "Full extraction took " << (int)((elapsed + extract_time) / (double)CLOCKS_PER_SEC * 1000.0) << "ms" << endl;

	gl_chunk.init(true, true);
	gl_chunk.set_data(v_out, i_out, flat_quads, smooth_shading);

	delete sampler.noise_sampler;
}

void DebugScene::init_binary_chunk()
{
	using namespace std;
	const int test_size = 256;
	//Sampler sampler = ImplicitFunctions::create_sampler(ImplicitFunctions::sphere);
	//sampler.block = ImplicitFunctions::torus_z_block;
	Sampler sampler;
	NoiseSamplers::create_sampler_noise3d(&sampler);
	cout << "Noise SIMD instruction set: " << get_simd_text() << endl;
	sampler.world_size = 256;

	SmartContainer<DualVertex> v_out(0);
	SmartContainer<uint32_t> i_out(262144);

	binary_chunk = new BinaryChunk(vec3(-test_size, -test_size, -test_size) * 0.5f, (float)test_size, 0, sampler, true);
	double extract_time = binary_chunk->extract(v_out, i_out, false);

	clock_t start_clock = clock();
	cout << "Processing...";

	int iters = 15;
	if (iters > 0 || !QUADS)
	{
		Processing::MeshProcessor<4> mp(true, SMOOTH_NORMALS);
		mp.init(v_out, i_out, sampler);
		mp.collapse_bad_quads();
		if (!QUADS)
		{
			v_out.count = 0;
			i_out.count = 0;
			mp.flush_to_tris(v_out, i_out);
			Processing::MeshProcessor<3> nmp = Processing::MeshProcessor<3>(true, SMOOTH_NORMALS);
			if (iters > 0)
			{
				nmp.init(v_out, i_out, sampler);
				nmp.optimize_dual_grid(iters);
				nmp.optimize_primal_grid(false, false);
				v_out.count = 0;
				i_out.count = 0;
				nmp.flush(v_out, i_out);
			}
		}
		else
		{
			mp.optimize_dual_grid(iters);
			mp.optimize_primal_grid(false, false);
			v_out.count = 0;
			i_out.count = 0;
			mp.flush(v_out, i_out);
		}
	}

	double elapsed = clock() - start_clock;
	cout << "done (" << (int)(elapsed / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
	cout << endl << "Full extraction took " << (int)((elapsed + extract_time) / (double)CLOCKS_PER_SEC * 1000.0) << "ms" << endl;

	gl_chunk.init(true, true);
	gl_chunk.set_data(v_out, i_out, flat_quads, smooth_shading);

	delete sampler.noise_sampler;
}

void DebugScene::init_world()
{
	world.init(256);
	world.init_updates(camera.v_position);
	/*world.split_leaves();
	world.extract_all();
	world.color_all();
	world.process_all();
	world.upload_all();*/

	update_required = false;

	update_thread = std::thread(std::bind(&DebugScene::watch_world_updates, this));

	last_extraction = clock();
}

int DebugScene::update(RenderInput* input)
{
	glfwPollEvents();
	camera.update(input);
	{
		std::unique_lock<std::mutex> l(world.watcher._mutex);
		world.watcher.focus_pos = camera.v_position;
	}

	return 0;
}

int DebugScene::render(RenderInput* input)
{
	glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//glUniform3fv(shader_eye_pos, 3,  camera.position);

	glUseProgram(shader_program);
	camera.set_shader(shader_projection, shader_view);

	render_world();
	//render_single_chunk();
	//render_binary_chunk();

	if (gui_visible)
		render_gui();

	glfwSwapBuffers(input->window);

	if (glfwGetKey(input->window, GLFW_KEY_ESCAPE))
	{
		glfwSetWindowShouldClose(input->window, 1);
	}

	return 0;
}

void DebugScene::render_single_chunk()
{
	if (fillmode == FILL_MODE_FILL || fillmode == FILL_MODE_BOTH)
	{
		//glEnable(GL_POLYGON_OFFSET_FILL);
		//glPolygonOffset(1.0f, 1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glCullFace(GL_FRONT);
		if (cull)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
		glUniform3f(shader_mul_clr, fill_color[0], fill_color[1], fill_color[2]);
		glUniform1f(shader_smooth_shading, (smooth_shading || flat_quads ? 1.0f : 0.0f));
		glUniform1f(shader_specular_power, specular_power);

		glBindVertexArray(gl_chunk.vao);
		if (!flat_quads || !QUADS)
			glDrawElements((QUADS ? GL_QUADS : GL_TRIANGLES), gl_chunk.p_count, GL_UNSIGNED_INT, 0);
		else
			glDrawArrays(GL_QUADS, 0, gl_chunk.p_count);

		//glDisable(GL_POLYGON_OFFSET_FILL);
	}
	if (fillmode == FILL_MODE_BOTH)
	{
		glUseProgram(outline_sp);
		camera.set_shader(outline_shader_projection, outline_shader_view);
		glLineWidth(line_width);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glCullFace(GL_FRONT);
		if (cull)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
		//glPolygonOffset(-0.1f, 1);
		glUniform3f(outline_shader_mul_clr, line_color[0], line_color[1], line_color[2]);

		glBindVertexArray(gl_chunk.vao);
		if (!flat_quads || !QUADS)
			glDrawElements((QUADS ? GL_QUADS : GL_TRIANGLES), gl_chunk.p_count, GL_UNSIGNED_INT, 0);
		else
			glDrawArrays(GL_QUADS, 0, gl_chunk.p_count);
	}

	glBindVertexArray(0);
}

void DebugScene::render_binary_chunk()
{
}

void DebugScene::render_world()
{
	/*{
		std::unique_lock<std::mutex> update_lock(update_mutex);
		if (update_required)
		{
			world.upload_all();
			update_required = false;
		}
	}*/
	std::unique_lock<std::mutex> draw_lock(world.watcher.renderables_mutex);
	if (!world.watcher.renderables_head)
		return;

	world.process_from_render_thread();

	/*for (auto& n : world.watcher.renderables)
	{
		if (n->delete_gl_chunk)
		{
			n->gl_chunk.destroy();
			n->delete_gl_chunk = false;
			if (delete_count++ >= MAX_DELETES)
				break;
		}
		else if (n->needs_upload)
		{
			n->upload();
			n->needs_upload = false;
			if (upload_count++ >= MAX_UPLOADS)
				break;
		}
	}*/

	if (fillmode == FILL_MODE_FILL || fillmode == FILL_MODE_BOTH)
	{
		//glEnable(GL_POLYGON_OFFSET_FILL);
		//glPolygonOffset(1.0f, 1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glCullFace(GL_FRONT);
		if (cull)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
		glUniform3f(shader_mul_clr, fill_color[0], fill_color[1], fill_color[2]);
		glUniform1f(shader_smooth_shading, (smooth_shading || flat_quads ? 1.0f : 0.0f));
		glUniform1f(shader_specular_power, specular_power);

		WorldOctreeNode* n = world.watcher.renderables_head;
		while (n)
		{
			int flags = n->flags;
			if (((flags & NODE_FLAGS_DRAW) || (flags & NODE_FLAGS_DRAW_CHILDREN)) && !(flags & NODE_FLAGS_GENERATING))
			{
				if (n->gl_chunk && n->gl_chunk->p_count != 0)
				{
					glBindVertexArray(n->gl_chunk->vao);
					if (!flat_quads || !QUADS)
						glDrawElements((QUADS ? GL_QUADS : GL_TRIANGLES), n->gl_chunk->p_count, GL_UNSIGNED_INT, 0);
					else
						glDrawArrays(GL_QUADS, 0, n->gl_chunk->p_count);
				}
			}
			n = n->renderable_next;
		}

		//glDisable(GL_POLYGON_OFFSET_FILL);
	}
	if (fillmode == FILL_MODE_BOTH)
	{
		glUseProgram(outline_sp);
		camera.set_shader(outline_shader_projection, outline_shader_view);
		glLineWidth(line_width);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glCullFace(GL_FRONT);
		if (cull)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
		//glPolygonOffset(-0.1f, 1);
		glUniform3f(outline_shader_mul_clr, line_color[0], line_color[1], line_color[2]);

		WorldOctreeNode* n = world.watcher.renderables_head;
		while (n)
		{
			int flags = n->flags;
			if (((flags & NODE_FLAGS_DRAW) || (flags & NODE_FLAGS_DRAW_CHILDREN)) && !(flags & NODE_FLAGS_GENERATING))
			{
				if (n->gl_chunk && n->gl_chunk->p_count != 0)
				{
					glBindVertexArray(n->gl_chunk->vao);
					if (!flat_quads || !QUADS)
						glDrawElements((QUADS ? GL_QUADS : GL_TRIANGLES), n->gl_chunk->p_count, GL_UNSIGNED_INT, 0);
					else
						glDrawArrays(GL_QUADS, 0, n->gl_chunk->p_count);
				}
			}
			n = n->renderable_next;
		}
	}

	glBindVertexArray(0);

	if (outline_visible)
	{
		glUseProgram(outline_sp);
		camera.set_shader(outline_shader_projection, outline_shader_view);
		glLineWidth(line_width);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);
		/*glUniform3f(shader_mul_clr, fill_color[0], fill_color[1], fill_color[2]);
		glUniform1f(shader_smooth_shading, 0);
		glUniform1f(shader_specular_power, 0);*/
		glUniform3f(outline_shader_mul_clr, line_color[0], line_color[1], line_color[2]);

		glBindVertexArray(world.outline_chunk.vao);
		glDrawElements(GL_LINES, world.outline_chunk.p_count, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
}

void DebugScene::key_callback(int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_F1)
		{
			gui_visible = !gui_visible;
		}
		if (key == GLFW_KEY_F2)
		{
			outline_visible = !outline_visible;
		}
		if (key == GLFW_KEY_F3)
		{
			fillmode = (fillmode == FILL_MODE_BOTH ? FILL_MODE_FILL : FILL_MODE_BOTH);
		}
		if (key == GLFW_KEY_F4)
		{
			cull = !cull;
		}

		if (key == GLFW_KEY_R)
		{
			camera.v_position = world.focus_point;
		}

		if (key == GLFW_KEY_SPACE)
		{
			world.destroy_leaves();
			world.focus_point = camera.v_position;
			world.init(256);
			world.split_leaves();
			world.extract_all();
			world.color_all();
			world.process_all();
			world.upload_all();
		}
	}
}

void DebugScene::render_gui()
{
	ImGui_ImplGlfwGL3_NewFrame();

	ImGui::Begin("Options", 0);

	uint32_t v_count = 0, p_count = 0;
	WorldOctreeNode* n = world.watcher.renderables_head;
	while (n)
	{
		if (n->gl_chunk && n->gl_chunk->p_count != 0)
		{
			v_count += n->gl_chunk->v_count;
			p_count += n->gl_chunk->p_count;
		}
		n = n->renderable_next;
	}

	ImGui::Text("Vertices: %i", v_count);
	ImGui::Text("Prims: %i", p_count / (QUADS ? 4 : 3));
	ImGui::Text("Leaves: %i", world.leaf_count);

	ImGui::Separator();

	ImGui::Text("Speed: %.2f", camera.speed);
	ImGui::Text("Pos: %.2f, %.2f, %.2f", camera.v_position.x, camera.v_position.y, camera.v_position.z);

	ImGui::Columns(2, 0, false);

	ImGui::Text("Smoothness:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_smoothness", &camera.smoothness, 0, 1.0f);
	ImGui::NextColumn();

	ImGui::Text("Mouse Sensitivity:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_sensitivity", &camera.rot_sensitivity, 0.0001f, 0.01f);
	ImGui::NextColumn();

	ImGui::Columns(1);

	CubicChunk* in_chunk = world.get_chunk_id_at(camera.v_position);
	ImGui::Text("Chunk: %i", (in_chunk ? in_chunk->id : -1));
	ImGui::Text("Node: %i", (in_chunk ? in_chunk->get_internal_node_at(camera.v_position) : -1));

	bool show_wireframe = fillmode == FILL_MODE_BOTH;
	ImGui::Checkbox("Wireframe", &show_wireframe);
	fillmode = (show_wireframe ? FILL_MODE_BOTH : FILL_MODE_FILL);

	ImGui::Text("Line width:", line_width);
	ImGui::SameLine();
	ImGui::PushItemWidth(100.0f); ImGui::SliderFloat("##lbl_line_width", &line_width, 1.0f, 5.0f, "%.1f", 0.5f); ImGui::PopItemWidth();

	ImGui::Text("Specular power:", line_width);
	ImGui::SameLine();
	ImGui::PushItemWidth(100.0f); ImGui::SliderFloat("##lbl_specular_power", &specular_power, 0.0f, 64.0f, "%.1f", 1.0f); ImGui::PopItemWidth();

	ImGui::Text("Colors:");
	ImGui::SameLine();
	if (ImGui::ColorButton("Line color", ImVec4(line_color[0], line_color[1], line_color[2], line_color[3])))
	{
		ImGui::OpenPopup("clr_line_picker");
	}
	if (ImGui::BeginPopup("clr_line_picker"))
	{
		ImGui::ColorEdit3("##lbl_clr_line", line_color);
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	if (ImGui::ColorButton("Fill color", ImVec4(fill_color[0], fill_color[1], fill_color[2], fill_color[3])))
	{
		ImGui::OpenPopup("clr_fill_picker");
	}
	if (ImGui::BeginPopup("clr_fill_picker"))
	{
		ImGui::ColorEdit3("##lbl_clr_fill", fill_color);
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	if (ImGui::ColorButton("Clear color", ImVec4(clear_color[0], clear_color[1], clear_color[2], clear_color[3])))
	{
		ImGui::OpenPopup("clr_clear_picker");
	}
	if (ImGui::BeginPopup("clr_clear_picker"))
	{
		ImGui::ColorEdit3("##lbl_clr_clear", clear_color);
		ImGui::EndPopup();
	}

	ImGui::Separator();


	ImGui::Columns(2, 0, false);

	ImGui::Text("Min depth:");
	ImGui::NextColumn();
	ImGui::SliderInt("##lbl_octree_min", &world.properties.min_level, 0, 32);
	ImGui::NextColumn();

	ImGui::Text("Max depth:");
	ImGui::NextColumn();
	ImGui::SliderInt("##lbl_octree_max", &world.properties.max_level, 1, 32);
	ImGui::NextColumn();

	ImGui::Text("Size modifier:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_octree_mod", &world.properties.size_modifier, 0.0f, 8.0f);
	ImGui::NextColumn();

	ImGui::Text("Split mult.:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_octree_split", &world.properties.split_multiplier, 1.0f, 4.0f);
	ImGui::NextColumn();

	ImGui::Text("Group mult.:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_octree_group", &world.properties.group_multiplier, 1.0f, 32.0f, 0, 0.5f);
	ImGui::NextColumn();

	ImGui::Text("Threads:");
	ImGui::NextColumn();
	if (ImGui::SliderInt("##lbl_threads", &world.properties.num_threads, 1, 16))
	{
		if (world.properties.num_threads == 0)
			world.properties.num_threads = std::thread::hardware_concurrency();
		omp_set_num_threads(world.properties.num_threads);
	}
	ImGui::NextColumn();

	ImGui::Text("Processes:");
	ImGui::NextColumn();
	ImGui::SliderInt("##lbl_processes", &world.properties.process_iters, 0, 100);
	ImGui::NextColumn();

	ImGui::Text("Resolution: %i", world.properties.chunk_resolution);
	ImGui::NextColumn();
	int mul = (int)log2((float)world.properties.chunk_resolution) - 4;
	ImGui::SliderInt("##lbl_resolution", &mul, 1, 4, 0);
	world.properties.chunk_resolution = (int)pow(2.0f, (float)(mul + 4));
	ImGui::NextColumn();


	ImGui::Separator();

	ImGui::Columns(1);
	ImGui::Checkbox("Quads", &quads);
	ImGui::Checkbox("Flat quads", &flat_quads);
	ImGui::Checkbox("Smooth shading", &smooth_shading);

	if (ImGui::Button("Extract All"))
	{
		world.destroy_leaves();
		world.init(256);
		world.split_leaves();
		world.extract_all();
		world.color_all();
		world.process_all();
		world.upload_all();
	}

	ImGui::End();

	ImGui::Render();
}

void DebugScene::watch_world_updates()
{

}
