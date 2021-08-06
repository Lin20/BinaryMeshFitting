# BinaryMeshFitting

Yet another attempt at making a fast massive level-of-detail voxel engine, but this time with usable results!

This is the successor to [PushingVoxelsForward](https://github.com/Lin20/PushingVoxelsForward).

## Building and Running

You can use CMake or the provided 64-bit Visual Studio 2017 solution.

### Dependencies

* [GLEW](http://glew.sourceforge.net/)
* [GLFW](http://www.glfw.org/)
* [Vc](https://github.com/VcDevel/Vc) (currently not actually used)
* [FastNoiseSIMD](https://github.com/Auburns/FastNoiseSIMD)

### CMake

So far only 64-bit building is tested on Windows using Visual Studio 2017.

The custom Find\* CMake modules are rather minimal:

* [FindGLM.cmake](cmake/Modules/FindGLM.cmake)
* [FindGLFW.cmake](cmake/Modules/FindGLFW.cmake)
* [FindFastNoiseSIMD.cmake](cmake/Modules/FindFastNoiseSIMD.cmake)

A slightly modified version of [cmake-precompiled-header](https://github.com/larsch/cmake-precompiled-header) is used.

#### [GLFW](http://www.glfw.org/) & [FastNoiseSIMD](https://github.com/Auburns/FastNoiseSIMD)

To support multi-configuration generators (e.g. Visual Studio 2017),
FindGLFW and FindFastNoiseSIMD expect separate "lib/Debug" and "lib/Release" directories
or a "d"-postfix for the debug library.

#### [Vc](https://github.com/VcDevel/Vc)

You may also want to change your
[VcConfig](https://github.com/VcDevel/Vc/blob/eef0f1f0588a06fa28cfd0e8810472db5e610ebe/cmake/VcConfig.cmake.in)
file from

```CMake
find_library(Vc_LIBRARIES Vc PATHS "${Vc_LIB_DIR}" NO_DEFAULT_PATH)
```

to something like

```CMake
find_library(Vc_LIBRARY_DEBUG Vcd PATHS "${Vc_LIB_DIR}" NO_DEFAULT_PATH)
find_library(Vc_LIBRARY_RELEASE Vc PATHS "${Vc_LIB_DIR}" NO_DEFAULT_PATH)
set(Vc_LIBRARIES debug ${Vc_LIBRARY_DEBUG} optimized ${Vc_LIBRARY_RELEASE})
```

to enable multi-configuration support.

## Media

![Colors](https://i.imgur.com/EwPzirl.png)
![Massive Generation](https://i.imgur.com/GlzJAYE.png)
![Massive Generation 2](https://i.imgur.com/O7CFm0o.png)

## Technical Details

This project uses a unique approach to extracting a mesh. Rather than rely on a density field or hermite data, it works off of binary data and processes it to form a smooth surface. This has numerous advantages, like reduced memory usage, compression, cache friendliness and more.

The technique used to process mesh is known as dual/primal mesh optimization, or the "dual of the dual operator", which was first described by Yutaka Ohtakey and Alexander G. Belyaev in _Dual/Primal Mesh Optimization for Polygonized Implicit Surfaces_. While their method uses marching cubes to extract a base mesh and an implicit function for refinement, Gregory Nielson later published a paper showing the same idea applied to a binary mesh in a cellular structure. Even after a small number of iterations, the mesh converges to the true surface.

While sharp features are lost with plain smoothing, passing in gradients allows the use of a QEF minimizer to restore sharp features and extract an optimal mesh. Additionally, the authors describe only processing to position vertices, but it can also be used to calculate smooth normals, blend colors and materials and more. The normals in the above milk-like screenshot started off with flat face normals from a cubic mesh.

Worth noting is that processing is an optional step. While dual marching cubes is the base algorithm, it's mixed with the octree traversal stage in (manifold) dual contouring, and as a result chunks can calculate and use hermite data to position vertices. This would result in a smooth mesh while foregoing processing, as well as introduce chunk/octree simplification if triangle count is a concern.

Lastly, this project can work with either triangles or quads. Quads are suggested because they result in far fewer primitives to be processed.

Here is an optimal mesh using the full technique that would normally appear jagged and deformed:

![Ideal Mesh](https://i.imgur.com/Vc2Y2wN.png)

## Efficiency

Speed is the biggest priority, and as such it's gone through many stages of optimization. Thanks to FastNoiseSIMD (linked above), generating noise is doable for fast fly-bys throughout the world. It also relies on pooling memory, multithreading, and clever bit tricks to speed up chunk extraction (see the article linked below for details) and run smoothly on any given hardware.

## To-Do

* [x] Dual marching cubes/manifold dual contouring
* [x] Level of detail
* [x] Stitching
* [x] World updating
* [x] Multithreaded extraction
* [x] Processing mesh
* [ ] Proper processing in-between chunks
* [ ] Using densities to position vertices
* [ ] Sharp feature support when gradients are supplied
* [ ] GPU offloading
* [ ] Realtime modification

## References

* Ju, Tao, Frank Losasso, Scott Schaefer, and Joe Warren. "Dual contouring of hermite data." In ACM transactions on graphics (TOG), vol. 21, no. 3, pp. 339-346. ACM, 2002.
* Schaefer, Scott, Tao Ju, and Joe Warren. "Manifold dual contouring." IEEE Transactions on Visualization and Computer Graphics 13, no. 3 (2007): 610-619.
* Ohtake, Yutaka, and Alexander G. Belyaev. "Dual/primal mesh optimization for polygonized implicit surfaces." In Proceedings of the seventh ACM symposium on Solid modeling and applications, pp. 171-178. ACM, 2002.
* Nielson, Gregory M.. “Dual marching cubes.” IEEE Visualization 2004 (2004): 489-496.

## Self-published article(s) describing techniques used

* [Lin. "Fast Cell Mask Building with Pseudo-SIMD Techniques." Voxelspace. December 12, 2017.](https://github.com/Lin20/BinaryMeshFitting/files/6947097/Fast.Cell.Mask.Building.Using.Pseudo-SIMD.Techniques.pdf)
