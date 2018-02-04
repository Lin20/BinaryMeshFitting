#pragma once

#include <FastNoiseSIMD.h>
#include "SmartContainer.hpp"
#include "Vertices.hpp"

class ColorMapper
{
public:
	FastNoiseSIMD* noise_context;

	ColorMapper();
	~ColorMapper();

	void generate_colors(SmartContainer<DualVertex>& verts);
	
private:
	void get_noise(SmartContainer<DualVertex>& verts, float** out);
	void map_noise(SmartContainer<DualVertex>& verts, float* noise);

	__forceinline glm::vec3 hsl_to_rgb(float h, float s, float l);
};
