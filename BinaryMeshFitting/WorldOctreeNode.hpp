#pragma once

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#define GLM_FORCE_NO_CTOR_INIT
#define GLM_FORCE_INLINE
#include <glm/glm.hpp>
#include <atomic>
#include "Vertices.hpp"
#include "Chunk.hpp"
#include "GLChunk.hpp"
#include "ResourceAllocator.hpp"

class CubicChunk;

typedef enum NODE_FLAGS
{
	NODE_FLAGS_NONE = 0,
	NODE_FLAGS_DRAW = 1,
	NODE_FLAGS_DESTROY = 2,
	NODE_FLAGS_SPLIT = 4,
	NODE_FLAGS_GROUP = 8,
	NODE_FLAGS_UPLOAD = 16,
	NODE_FLAGS_DIRTY = 32,
	NODE_FLAGS_DRAW_CHILDREN = 64,
	NODE_FLAGS_GENERATING = 128,
	NODE_FLAGS_SUPERCEDED = 256
};

typedef enum GENERATION_STAGES
{
	GENERATION_STAGES_UNHANDLED = 0,
	GENERATION_STAGES_WATCHER_QUEUED = 1,
	GENERATION_STAGES_GENERATOR_QUEUED = 2,
	GENERATION_STAGES_GENERATOR_ACKNOWLEDGED = 3,
	GENERATION_STAGES_GENERATING = 4,
	GENERATION_STAGES_NEEDS_FORMAT = 5,
	GENERATION_STAGES_NEEDS_UPLOAD = 6,
	GENERATION_STAGES_UPLOADING = 7,
	GENERATION_STAGES_AWAITING_STITCHING = 8,
	GENERATION_STAGES_AWAITING_STITCHING_UPLOAD = 9,
	GENERATION_STAGES_DONE = 10
};

struct OctreeNode
{
	bool world_node_flag;
	bool leaf_flag;
	uint32_t index;
	float size;
	uint8_t level;
	glm::vec3 pos;
	OctreeNode* parent;
	OctreeNode* children[8];

	inline OctreeNode() : OctreeNode(0, 0, 0, glm::vec3(0, 0, 0), 0) {}
	inline OctreeNode(uint32_t _index, OctreeNode* _parent, float _size, glm::vec3 _pos, uint8_t _level) : index(_index), parent(_parent), size(_size), pos(_pos), level(_level), children{ 0,0,0,0,0,0,0,0 } {}

	void generate_outline(SmartContainer<glm::vec3>& v_pos, SmartContainer<uint32_t>& inds);
	OctreeNode* get_node_at(glm::vec3 p);

	inline const bool is_leaf() { return leaf_flag; }
	inline const bool is_world_node() { return world_node_flag; }
};

class WorldOctreeNode : public OctreeNode
{
public:
	WorldOctreeNode* renderable_prev;
	WorldOctreeNode* renderable_next;
	std::atomic<int> flags;
	std::atomic<int> generation_stage;
	CubicChunk* chunk;
	glm::vec3 middle;
	bool world_leaf_flag;
	bool force_chunk_octree;
	GLChunk* gl_chunk;

	WorldOctreeNode();
	WorldOctreeNode(uint32_t _index, WorldOctreeNode* _parent, float _size, glm::vec3 _pos, uint8_t _level);
	WorldOctreeNode(uint32_t _index);
	~WorldOctreeNode();

	void init(uint32_t _index, WorldOctreeNode* _parent, float _size, glm::vec3 _pos, uint8_t _level);

	bool format(ResourceAllocator<GLChunk>* allocator);
	bool upload();
	void unlink();
};

class DualNode : public OctreeNode
{
public:
	uint32_t code;
	CubicChunk* root;
	uint32_t i_size;
	glm::ivec3 xyz;
	Cell cell;

	DualNode();
	DualNode(CubicChunk* _chunk, uint32_t _index, float _size, glm::vec3 _pos, glm::ivec3 _xyz, uint8_t _level, uint32_t _int_size, Cell* _cell);

	inline bool operator==(const DualNode& other) const
	{
		return other.level == level && other.xyz == xyz;
	}
};

namespace std
{
	template <>
	struct hash<DualNode>
	{
		size_t operator()(const DualNode& e) const
		{
			return hash<uint32_t>{}(e.code);
		}
	};
}
