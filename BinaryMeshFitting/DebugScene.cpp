#include "PCH.h"
#include "DebugScene.hpp"
#include "Core.hpp"
#include "DefaultOptions.h"
#include "ImplicitSampler.hpp"
#include "NoiseSampler.hpp"
#include "MeshProcessor.hpp"
#include "DMCChunk.hpp"
#include <time.h>
#include <iostream>
#include <omp.h>
#include <glm/ext.hpp>
#include <fstream>
#include <streambuf>

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
	this->world_visible = true;
	this->update_focus = true;
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

	create_texture("rock.jpg", rock_texture);
	create_texture("rock2.jpg", rock2_texture);
	create_texture("grass.jpg", grass_texture);

	unsigned char* pixels = (unsigned char*)malloc(16 * 16 * 16 * 3);
	for (int i = 0; i < 16 * 16 * 16 * 3; i++)
	{
		char c = rand() % 256;
		pixels[i] = c;
	}
	noise_texture.init(16, 16, 16, pixels);

	GLint success;
	GLint log_size = 0;

	load_main_shader();

	this->outline_sp = glCreateProgram();
	glAttachShader(this->outline_sp, this->outline_fs);
	glAttachShader(this->outline_sp, this->outline_vs);

	glBindAttribLocation(this->outline_sp, 0, "vertex_position");

	glLinkProgram(this->outline_sp);
	LINKER_ERROR_CHECK(this->outline_sp, "outline shader");
	this->outline_shader_projection = glGetUniformLocation(this->outline_sp, "projection");
	this->outline_shader_view = glGetUniformLocation(this->outline_sp, "view");
	this->outline_shader_mul_clr = glGetUniformLocation(this->outline_sp, "mul_color");
	this->outline_shader_camera_pos = glGetUniformLocation(this->outline_sp, "camera_pos");
	this->outline_shader_chunk_pos = glGetUniformLocation(this->outline_sp, "chunk_pos");

	this->camera.init(render_input->width, render_input->height, render_input);
	this->camera.set_shader(this->shader_projection, this->shader_view);

	printf("Initializing imgui...");
	ImGui_ImplGlfwGL3_Init(render_input->window, true);
	ImGui::StyleColorsLight();
	ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.85f;
	printf("Done.\n");

	dmc_chunk = 0;

	init_world();
	//init_dmc_chunk();
}

DebugScene::~DebugScene()
{
	world.watcher.stop();

	delete dmc_chunk;
	ImGui_ImplGlfwGL3_Shutdown();
}

bool DebugScene::create_texture(std::string filename, Texture& out)
{
	std::string paths[4] = { "", "C:/textures/", "./textures/", "../../BinaryMeshFitting/textures/" };

	std::cout << "Loading " << filename << "...";

	for (int i = 0; i < 4; i++)
	{
		std::string local_path = paths[i];
		local_path.append(filename);
		if (out.load_from_file(local_path))
			break;
	}

	if (out.initialized)
		std::cout << "Success." << std::endl;
	else
		std::cout << "Failed." << std::endl;

	return out.initialized;
}

bool DebugScene::create_shader(std::string data, GLuint* out, GLenum type, const char* name)
{
	std::cout << "Compiling " << name << "...";
	if (!out || data.size() == 0)
	{
		std::cout << "Failed because empty." << std::endl;
		return false;
	}

	GLint success;
	GLint log_size = 0;
	const char* str = data.c_str();
	*out = glCreateShader(type);
	glShaderSource(*out, 1, &str, NULL);
	glCompileShader(*out);
	SHADER_ERROR_CHECK(*out, name);

	if (success != GL_FALSE)
		std::cout << "Success." << std::endl;

	return success != GL_FALSE;
}

bool DebugScene::create_shader_from_file(std::string filename, GLuint* out, GLenum type, const char* name)
{
	std::string paths[3] = { "C:/shaders/", "./shaders/", "../../BinaryMeshFitting/shaders/" };
	std::ifstream t(filename);
	if (t.fail())
	{
		for (int i = 0; i < 3; i++)
		{
			std::string local_filename = std::string(paths[i]);
			local_filename.append(filename);
			t = std::ifstream(local_filename);
			if (!t.fail())
				break;
		}
	}

	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	return create_shader(str, out, type, name);
}

void DebugScene::load_main_shader()
{
	GLint success;
	GLint log_size = 0;

	create_shader_from_file("main_vs.glsl", &vertex_shader, GL_VERTEX_SHADER, "regular vs");
	create_shader_from_file("main_fs.glsl", &fragment_shader, GL_FRAGMENT_SHADER, "regular fs");

	create_shader_from_file("outline_vs.glsl", &outline_vs, GL_VERTEX_SHADER, "outline vs");
	create_shader_from_file("outline_fs.glsl", &outline_fs, GL_FRAGMENT_SHADER, "outline fs");

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
	this->shader_camera_pos = glGetUniformLocation(this->shader_program, "camera_pos");
	this->shader_chunk_pos = glGetUniformLocation(this->shader_program, "chunk_pos");
	this->shader_chunk_depth = glGetUniformLocation(this->shader_program, "chunk_depth");
	this->shader_rock_texture = glGetUniformLocation(this->shader_program, "rock_texture");
	this->shader_rock2_texture = glGetUniformLocation(this->shader_program, "rock2_texture");
	this->shader_grass_texture = glGetUniformLocation(this->shader_program, "grass_texture");
	this->shader_noise_texture = glGetUniformLocation(this->shader_program, "noise_texture");
}

void DebugScene::init_dmc_chunk()
{
	using namespace std;
	const int test_size = 256;
	//Sampler sampler = ImplicitFunctions::create_sampler(ImplicitFunctions::plane_y);
	//sampler.block = ImplicitFunctions::torus_z_block;
	Sampler sampler;
	NoiseSamplers::create_sampler_terrain_pert_3d(&sampler);
	cout << "Noise SIMD instruction set: " << get_simd_text() << endl;
	sampler.world_size = 256;

	SmartContainer<DualVertex> v_out(0);
	SmartContainer<uint32_t> i_out(262144);

	dmc_chunk = new DMCChunk(vec3(-test_size, -test_size, -test_size) * 0.5f, (float)test_size, 0, sampler, 1);
	double extract_time = dmc_chunk->extract(v_out, i_out, false);

	/*cout << "Processing...";
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
	}*/

	gl_chunk.format_data(v_out, i_out, flat_quads, smooth_shading);

	cout << endl << "Full extraction took " << (int)((extract_time) / (double)CLOCKS_PER_SEC * 1000.0) << "ms" << endl;

	gl_chunk.init(true, true);
	gl_chunk.format_data(v_out, i_out, false, smooth_shading);
	gl_chunk.set_data(gl_chunk.p_data, gl_chunk.c_data, &i_out);

	for(int i = 0; i < 8; i++)
		delete sampler.noise_samplers[i];
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

	last_extraction = clock();
}

int DebugScene::update(RenderInput* input)
{
	glfwPollEvents();
	camera.update(input);
	if (update_focus)
	{
		std::unique_lock<std::mutex> l(world.watcher._mutex);
		world.watcher.focus_pos = camera.v_position + camera.v_velocity * 16.0f;
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
	glUniform3f(shader_camera_pos, camera.v_position[0], camera.v_position[1], camera.v_position[2]);

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

void DebugScene::render_dmc_chunk()
{
}

void DebugScene::render_world()
{
	std::unique_lock<std::mutex> draw_lock(world.watcher.renderables_mutex);
	if (!world.watcher.renderables_head)
		return;

	world.process_from_render_thread();

	if (!world_visible)
		return;

	frustum.CalculateFrustum(value_ptr(camera.mat_projection), value_ptr(camera.mat_view_frustum));

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

		glUniform1i(shader_rock_texture, 0);
		glUniform1i(shader_rock2_texture, 1);
		glUniform1i(shader_grass_texture, 2);
		glUniform1i(shader_noise_texture, 3);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rock_texture.id);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rock2_texture.id);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, grass_texture.id);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_3D, noise_texture.id);

		WorldOctreeNode* n = world.watcher.renderables_head;
		while (n)
		{
			int flags = n->flags;
			if (flags & NODE_FLAGS_DRAW)
			{
				if (n->gl_chunk && n->gl_chunk->p_count != 0 && frustum.CubeInFrustum(n->chunk->bound_start.x, n->chunk->bound_start.y, n->chunk->bound_start.z, n->chunk->bound_size))
				{
					glUniform4f(shader_chunk_pos, n->chunk->overlap_pos.x, n->chunk->overlap_pos.y, n->chunk->overlap_pos.z, n->chunk->scale);
					glUniform1f(shader_chunk_depth, n->level);
					glBindVertexArray(n->gl_chunk->vao);
					if (!flat_quads || !QUADS)
						glDrawElements((QUADS ? GL_QUADS : GL_TRIANGLES), n->gl_chunk->p_count, GL_UNSIGNED_INT, 0);
					else
						glDrawArrays(GL_QUADS, 0, n->gl_chunk->p_count);
				}
			}
			n = n->renderable_next;
		}

		if (world.properties.enable_stitching)
		{
			glBindVertexArray(world.watcher.generator.stitcher.gl_chunk.vao);
			glDrawArrays(GL_TRIANGLES, 0, world.watcher.generator.stitcher.gl_chunk.v_count);
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_3D, 0);

		//glDisable(GL_POLYGON_OFFSET_FILL);
	}
	if (fillmode == FILL_MODE_BOTH)
	{
		glUseProgram(outline_sp);
		camera.set_shader(outline_shader_projection, outline_shader_view);
		glUniform3f(outline_shader_camera_pos, camera.v_position[0], camera.v_position[1], camera.v_position[2]);

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
			if (flags & NODE_FLAGS_DRAW)
			{
				if (n->gl_chunk && n->gl_chunk->p_count != 0 && frustum.CubeInFrustum(n->chunk->bound_start.x, n->chunk->bound_start.y, n->chunk->bound_start.z, n->chunk->bound_size))
				{
					glUniform4f(outline_shader_chunk_pos, n->chunk->overlap_pos.x, n->chunk->overlap_pos.y, n->chunk->overlap_pos.z, n->chunk->scale);
					glBindVertexArray(n->gl_chunk->vao);
					if (!flat_quads || !QUADS)
						glDrawElements((QUADS ? GL_QUADS : GL_TRIANGLES), n->gl_chunk->p_count, GL_UNSIGNED_INT, 0);
					else
						glDrawArrays(GL_QUADS, 0, n->gl_chunk->p_count);
				}
			}
			n = n->renderable_next;
		}

		if (world.properties.enable_stitching)
		{
			glBindVertexArray(world.watcher.generator.stitcher.gl_chunk.vao);
			glDrawArrays(GL_TRIANGLES, 0, world.watcher.generator.stitcher.gl_chunk.v_count);
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
		if (key == GLFW_KEY_F5)
		{
			world_visible = !world_visible;
		}
		if (key == GLFW_KEY_F6)
		{
			reload_shaders();
		}

		if (key == GLFW_KEY_R)
		{
			camera.v_position = world.focus_point;
		}
		if (key == GLFW_KEY_SPACE)
		{
			update_focus = !update_focus;
		}

		if (key == GLFW_KEY_PAGE_UP)
		{
			world.properties.num_threads = min(16, world.properties.num_threads + 1);
		}
		if (key == GLFW_KEY_PAGE_DOWN)
		{
			world.properties.num_threads = max(1, world.properties.num_threads - 1);
		}
		if (key == GLFW_KEY_KP_ADD)
		{
			if (!(mods & GLFW_MOD_SHIFT))
				world.properties.max_level = min(32, world.properties.max_level + 1);
			else
				world.properties.min_level = min(32, world.properties.min_level + 1);
		}
		if (key == GLFW_KEY_KP_SUBTRACT)
		{
			if (!(mods & GLFW_MOD_SHIFT))
				world.properties.max_level = max(1, world.properties.max_level - 1);
			else
				world.properties.min_level = max(1, world.properties.min_level - 1);
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

	DMCChunk* in_chunk = world.get_chunk_id_at(camera.v_position);
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
	ImGui::SliderFloat("##lbl_octree_mod", &world.properties.size_modifier, 0.0f, 64.0f);
	ImGui::NextColumn();

	ImGui::Text("Split mult.:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_octree_split", &world.properties.split_multiplier, 1.0f, 8.0f);
	ImGui::NextColumn();

	ImGui::Text("Group mult.:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_octree_group", &world.properties.group_multiplier, 1.0f, 8.0f);
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
	ImGui::SliderInt("##lbl_processes", &world.properties.process_iters, 0, 10);
	ImGui::NextColumn();

	ImGui::Text("Resolution: %i", world.properties.chunk_resolution);
	ImGui::NextColumn();
	int mul = (int)log2((float)world.properties.chunk_resolution) - 4;
	ImGui::SliderInt("##lbl_resolution", &mul, 1, 4, 0);
	world.properties.chunk_resolution = (int)pow(2.0f, (float)(mul + 4));
	ImGui::NextColumn();

	ImGui::Text("Overlap:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_overlap", &world.properties.overlap, 0.0f, 0.1f);
	ImGui::NextColumn();

	ImGui::Separator();

	ImGui::Columns(1);
	ImGui::Checkbox("Boundary Processing", &world.properties.boundary_processing);
	//ImGui::Checkbox("Quads", &quads);
	//ImGui::Checkbox("Flat quads", &flat_quads);
	ImGui::Checkbox("Smooth shading", &smooth_shading);
	ImGui::Checkbox("Stitching", &world.properties.enable_stitching);
	ImGui::Checkbox("Update focus point", &update_focus);

	ImGui::Separator();

	if (ImGui::Button("Reload Shaders"))
	{
		reload_shaders();
	}

	ImGui::End();



	ImGui::Begin("Noise");

	ImGui::Columns(2, 0, false);

	ImGui::Text("Scale:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_noise_scale", &world.noise_properties.g_scale, 0.0625f, 1.0f);
	ImGui::NextColumn();

	ImGui::Text("Height:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_noise_height", &world.noise_properties.height, 16.0f, 128.0f);
	ImGui::NextColumn();

	ImGui::Text("Octaves:");
	ImGui::NextColumn();
	ImGui::SliderInt("##lbl_noise_octaves", &world.noise_properties.octaves, 1, 32);
	ImGui::NextColumn();

	ImGui::Text("Amp:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_noise_amp", &world.noise_properties.amp, 0.0f, 4.0f);
	ImGui::NextColumn();

	ImGui::Text("Frequency:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_noise_freq", &world.noise_properties.frequency, 0.0f, 4.0f);
	ImGui::NextColumn();

	ImGui::Text("Gain:");
	ImGui::NextColumn();
	ImGui::SliderFloat("##lbl_noise_gain", &world.noise_properties.gain, 0.42f, 0.52);
	ImGui::NextColumn();

	ImGui::End();



	ImGui::Render();
}

void DebugScene::reload_shaders()
{
	std::cout << "Reloading shaders." << std::endl;

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	glDeleteShader(shader_program);

	load_main_shader();
}

