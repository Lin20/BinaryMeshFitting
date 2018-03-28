#pragma once
#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

class FPSCamera
{
public:
	int last_1 : 1;
	int last_2 : 1;
	int last_c : 1;
	int last_mb : 1;
	int lock_cursor : 1;
	float speed;
	float smoothness;
	float rot_sensitivity;
	double last_x;
	double last_y;
	int last_modifiers[3];

	glm::vec3 v_position;
	glm::vec3 v_rot;
	glm::vec3 v_velocity;
	glm::vec3 v_turn_velocity;

	glm::mat4 mat_projection;
	glm::mat4 mat_view;
	glm::mat4 mat_view_frustum;
	glm::mat4 mat_rotation;
	glm::quat camera_quat;

	void init(uint32_t width, uint32_t height, struct RenderInput* render_input);
	void update(struct RenderInput* render_input);
	void set_shader(GLint shader_proj, GLint shader_view);
};
