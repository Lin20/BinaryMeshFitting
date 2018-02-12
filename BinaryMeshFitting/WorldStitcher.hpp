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

typedef enum STITCHING_OPERATIONS
{
	STITCHING_OPERATIONS_CELL = 0,
	STITCHING_OPERATIONS_FACE = 1,
	STITCHING_OPERATIONS_EDGE = 2,
	STITCHING_OPERATIONS_INDEXES = 3
};

struct StitchOperation
{
	STITCHING_OPERATIONS op;
	int dir;
	class OctreeNode* cells[4];

	StitchOperation()
	{
		op = STITCHING_OPERATIONS_CELL;
		dir = 0;
		cells[0] = 0;
		cells[1] = 0;
		cells[2] = 0;
		cells[3] = 0;
	}

	StitchOperation(STITCHING_OPERATIONS _op, int _dir, class OctreeNode** _cells)
	{
		op = _op;
		dir = _dir;
		switch (_op)
		{
		case STITCHING_OPERATIONS_CELL:
			cells[0] = _cells[0];
			break;

		case STITCHING_OPERATIONS_FACE:
			cells[0] = _cells[0];
			cells[1] = _cells[1];
			break;

		case STITCHING_OPERATIONS_EDGE:
		case STITCHING_OPERATIONS_INDEXES:
			cells[0] = _cells[0];
			cells[1] = _cells[1];
			cells[2] = _cells[2];
			cells[3] = _cells[3];
			break;
		}
	}
};

class WorldStitcher
{
public:
	WorldStitcher();
	~WorldStitcher();

	void init();
	void stitch_all(class WorldOctreeNode* root);
	void upload();
	void format();

	std::mutex _mutex;
	std::atomic<int> stage;

	GLChunk gl_chunk;
	int gl_index;

	std::deque<StitchOperation> queue;

private:
	SmartContainer<DualVertex> vertices;

	void stitch_cell(class OctreeNode* n, SmartContainer<DualVertex>& v_out);
	void stitch_faces(class OctreeNode* n[2], int direction, SmartContainer<DualVertex>& v_out);
	void stitch_edges(class OctreeNode* n[4], int direction, SmartContainer<DualVertex>& v_out);
	void stitch_indexes(class OctreeNode* n[4], int direction, SmartContainer<DualVertex>& v_out);
};
