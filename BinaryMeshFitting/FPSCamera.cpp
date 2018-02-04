#include "PCH.h"
#include "FPSCamera.hpp"
#include "Core.hpp"

#include <glm/ext.hpp>

using namespace glm;

const float DEFAULT_SMOOTHNESS = 0.85f;
const float DEFAULT_ROTATION_SENSITIVITY = 0.001f;

void FPSCamera::init(uint32_t width, uint32_t height, RenderInput* render_input)
{
	this->last_1 = 0;
	this->last_2 = 0;
	this->lock_cursor = 0;
	this->last_mb = 0;
	this->speed = 0.1f;
	this->smoothness = DEFAULT_SMOOTHNESS;
	this->rot_sensitivity = DEFAULT_ROTATION_SENSITIVITY;
	this->v_position = vec3(0, 0, 0);
	this->v_rot = vec3(0, 0, 0);
	this->v_velocity = vec3(0, 0, 0);
	this->v_turn_velocity = vec3(0, 0, 0);

	this->camera_quat = glm::quat(v_rot);
	this->mat_rotation = yawPitchRoll(v_rot.x, v_rot.y, v_rot.z);
	this->mat_projection = perspective(PI / 3.0f, (float)width / (float)height, 0.0001f, 10000.0f);
	
	glfwSetCursorPos(render_input->window, render_input->width * 0.5, render_input->height * 0.5);
	glfwGetCursorPos(render_input->window, &this->last_x, &this->last_y);

	this->update(render_input);
}

void FPSCamera::update(RenderInput* render_input)
{
	if (glfwGetKey(render_input->window, GLFW_KEY_1) && !last_1)
		speed *= 2.0f;
	if (glfwGetKey(render_input->window, GLFW_KEY_2) && !last_2)
		speed *= 0.5f;
	last_1 = glfwGetKey(render_input->window, GLFW_KEY_1);
	last_2 = glfwGetKey(render_input->window, GLFW_KEY_2);

	if (glfwGetMouseButton(render_input->window, GLFW_MOUSE_BUTTON_MIDDLE) && !last_mb)
		lock_cursor = !lock_cursor;
	last_mb = glfwGetMouseButton(render_input->window, GLFW_MOUSE_BUTTON_MIDDLE);

	float move_speed = speed;
	float delta_smoothness = smoothness;
	if (glfwGetKey(render_input->window, GLFW_KEY_LEFT_SHIFT))
		move_speed *= 5.0f;

	if (glfwGetKey(render_input->window, GLFW_KEY_W))
	{
		v_velocity += vec3(mat_rotation * vec4(0, 0, move_speed, 0));
	}
	if (glfwGetKey(render_input->window, GLFW_KEY_S))
	{
		v_velocity += vec3(mat_rotation * vec4(0, 0, -move_speed, 0));
	}
	if (glfwGetKey(render_input->window, GLFW_KEY_D))
	{
		v_velocity += vec3(mat_rotation * vec4(-move_speed, 0, 0, 0));
	}
	if (glfwGetKey(render_input->window, GLFW_KEY_A))
	{
		v_velocity += vec3(mat_rotation * vec4(move_speed, 0, 0, 0));
	}

	v_position += v_velocity;
	v_velocity *= delta_smoothness;

	// Rotate the camera
	float dx, dy;
	if (glfwGetWindowAttrib(render_input->window, GLFW_FOCUSED) && (lock_cursor || glfwGetMouseButton(render_input->window, GLFW_MOUSE_BUTTON_RIGHT) || glfwGetKey(render_input->window, GLFW_KEY_LEFT_CONTROL)))
	{
		glfwSetInputMode(render_input->window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		double current_x, current_y;
		glfwGetCursorPos(render_input->window, &current_x, &current_y);
		if (lock_cursor)
			glfwSetCursorPos(render_input->window, render_input->width * 0.5, render_input->height * 0.5);
		else
			glfwSetCursorPos(render_input->window, last_x, last_y);
		dx = (float)(current_x - last_x);
		dy = (float)(current_y - last_y);
		glfwGetCursorPos(render_input->window, &last_x, &last_y);
	}
	else
	{
		dx = 0;
		dy = 0;
		glfwGetCursorPos(render_input->window, &last_x, &last_y);
		glfwSetInputMode(render_input->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	
	v_turn_velocity += vec3(rot_sensitivity * -dx, rot_sensitivity * dy, 0);
	v_rot += v_turn_velocity;
	v_turn_velocity *= smoothness;

	/*mat_rotation *= eulerAngleXYZ(v_turn_velocity.x, v_turn_velocity.y, v_turn_velocity.z);
	vec3 target = eulerAngleXYZ(v_rot.x, v_rot.y, v_rot.z) * vec4(0, 0, 1, 0);
	target += v_position;
	vec3 up = mat_rotation * vec4(0, 1, 0, 0);
	mat_view = lookAt(v_position, target, up);*/
	//mat4 translation = translate(mat4(1.0f), -v_position);
	//mat_view = mat_rotation * translation;

	vec3 target, up;
	mat_rotation = yawPitchRoll(v_rot.x, v_rot.y, v_rot.z);
	target = mat_rotation * vec4(0, 0, 1, 1);
	up = mat_rotation * vec4(0, 1, 0, 1);
	target += v_position;

	mat_view = lookAt(v_position, target, up);
}

void FPSCamera::set_shader(GLint shader_proj, GLint shader_view)
{
	glUniformMatrix4fv(shader_proj, 1, GL_FALSE, value_ptr(this->mat_projection));
	glUniformMatrix4fv(shader_view, 1, GL_FALSE, value_ptr(this->mat_view));
}
