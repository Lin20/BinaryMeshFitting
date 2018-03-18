#pragma once

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#define GLM_FORCE_NO_CTOR_INIT
#define GLM_FORCE_INLINE
#include <glm/glm.hpp>
#include <atomic>
#include "Vertices.hpp"
#include "GLChunk.hpp"
#include "ResourceAllocator.hpp"

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

struct MortonCode
{
	uint64_t code;

	MortonCode() { code = 0; }
	MortonCode(uint64_t _code) : code(_code) {}

	inline bool operator==(const MortonCode& other) const
	{
		return code == other.code;
	}

	inline bool operator==(const uint64_t other) const
	{
		return code == other;
	}

	inline MortonCode& operator=(const MortonCode& other)
	{
		code = other.code;
		return *this;
	}

	inline MortonCode& operator=(const uint64_t other)
	{
		code = other;
		return *this;
	}
};

namespace std
{
	template<>
	struct hash<MortonCode>
	{
		__forceinline size_t operator()(const MortonCode& o) const
		{
			uint32_t a = ((o.code & 0xFFFFFFFF) ^ (o.code >> 32));
			a = (a ^ 61) ^ (a >> 16);
			a = a + (a << 3);
			a = a ^ (a >> 4);
			a = a * 0x27d4eb2d;
			a = a ^ (a >> 15);
			return a;
			//uint64_t morton_code = o.code;
			//return std::hash<uint64_t>::_Do_hash(morton_code);
		}
	};
}

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

	MortonCode morton_code;

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
	class DMCChunk* chunk;
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

class DMCNode : public OctreeNode
{
public:
	DMCChunk* root;
	uint32_t i_size;
	glm::ivec3 xyz;
	float sample;

	DMCNode();
	DMCNode(DMCChunk* _chunk, float _size, glm::vec3 _pos, glm::ivec3 _xyz, uint8_t _level, uint32_t _int_size, float _sample);

	inline bool operator==(const DMCNode& other) const
	{
		return morton_code == other.morton_code;
	}
};
