#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <list>
#include <deque>
#include <glm/glm.hpp>
#include "ThreadDebug.hpp"
#include "SmartContainer.hpp"
#include "GLChunk.hpp"
#include "HashMap.hpp"
#include "WorldOctreeNode.hpp"

typedef enum STITCHING_STAGES
{
	STITCHING_STAGES_READY = 0,
	STITCHING_STAGES_STITCHING = 1,
	STITCHING_STAGES_AWAITING_SYNC = 2,
	STITCHING_STAGES_NEEDS_ACKNOWLEDGEMENT = 3,
	STITCHING_STAGES_ACKNOWLEDGED = 4,
	STITCHING_STAGES_NEEDS_UPLOAD = 5,
	STITCHING_STAGES_UPLOADING = 6,
	STITCHING_STAGES_UPLOADED = 7
};

class WorldStitcher
{
public:
	WorldStitcher();
	~WorldStitcher();

	void init();
	void stitch_all(class WorldOctreeNode* root);
	void stitch_leaves(SmartContainer<WorldOctreeNode*> chunk_leaves, emilib::HashMap<MortonCode, class DMCNode*>& leaves);
	void upload();
	void format();

	std::mutex _mutex;
	std::atomic<int> stage;

	GLChunk gl_chunk;
	int gl_index;

private:
	SmartContainer<DualVertex> vertices;

	void stitch_cell(class OctreeNode* n, SmartContainer<DualVertex>& v_out);

	void stitch_face_xy(class OctreeNode* n0, class OctreeNode* n1, SmartContainer<DualVertex>& v_out);
	void stitch_face_zy(class OctreeNode* n0, class OctreeNode* n1, SmartContainer<DualVertex>& v_out);
	void stitch_face_xz(class OctreeNode* n0, class OctreeNode* n1, SmartContainer<DualVertex>& v_out);

	void stitch_edge_x(class OctreeNode* n0, class  OctreeNode* n1, class OctreeNode* n2, class  OctreeNode* n3, SmartContainer<DualVertex>& v_out);
	void stitch_edge_y(class OctreeNode* n0, class OctreeNode* n1, class  OctreeNode* n2, class OctreeNode* n3, SmartContainer<DualVertex>& v_out);
	void stitch_edge_z(class OctreeNode* n0, class  OctreeNode* n1, class  OctreeNode* n2, class OctreeNode* n3, SmartContainer<DualVertex>& v_out);

	void stitch_indexes(class OctreeNode* n[8], SmartContainer<DualVertex>& v_out);

	void stitch_leaf(DMCNode* n, emilib::HashMap<MortonCode, class DMCNode*>& leaves, SmartContainer<DualVertex>& v_out);

	int key2level(uint64_t key);
	void leaf2vert(uint64_t k, uint64_t* v_out, int* lv);
	void vert2leaf(uint64_t v, int lv, uint64_t* n_out);
	void stitch_mc(DMCNode* nodes[8], SmartContainer<DualVertex>& v_out);
};
