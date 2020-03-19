// make sure we can use dispatch loader dynamic
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include "common/scene.h"
#include "common/camera.h"

#include "common/bluenoise.h"

#include "renderer/rtao.h"
#include "renderer/primpath.h"

int
main()
{
	RgbaImage2d<float> hdr_envmap("envmap/palermo_sidewalk_1k.hdr");

	int num_spp = 1;

	// setup the scene
	Scene scene;
	//scene.add_mesh("bistro/exterior/exterior.obj", true);
	//scene.add_mesh("test/mitsuba-sphere.obj", true);
	//scene.add_mesh("sponza/sponza.obj", true);
	scene.add_mesh("fireplace_room/fireplace_room.obj", true);
	//scene.add_mesh("bunny_box/bunny_box.obj", true);
	scene.build_rt();

	// setup the camera
	const vec3 look_from = scene.m_bbox_max * 1.1f;
	const vec3 look_at = (scene.m_bbox_max + scene.m_bbox_min) * 0.5f;
	const vec3 up = vec3(0.0f, 1.0f, 0.0f);
	FpsCamera camera(look_from,
					 look_at,
					 up,
					 glm::radians(45.0f),
					 float(Extent.width) / float(Extent.height));
	camera.m_move_speed = length(scene.m_bbox_max - scene.m_bbox_min) * 0.08f;
	camera.m_up_speed = length(scene.m_bbox_max - scene.m_bbox_min) * 0.08f;

	// run renderer
#if 0
	Rtao rtao;
	rtao.run(&scene, &camera);
#else
	PrimitivePathTracer prim_path_tracer;
	prim_path_tracer.run(&scene, &camera, hdr_envmap);
#endif

	return 0;
}