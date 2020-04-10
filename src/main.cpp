// make sure we can use dispatch loader dynamic
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include "common/scene.h"
#include "common/camera.h"

#include "common/bluenoise.h"

#include "renderer_app/rtao.h"
#include "renderer_app/pathtracer.h"
#include "misc_app/probeplacer_silvennoinen17.h"

int
main()
{
	// setup the scene
	Scene scene;
	scene.set_envmap("envmap/palermo_sidewalk_1k.hdr", true);
	//scene.add_mesh("scenes/bistro/exterior/exterior.obj", true);
	scene.add_mesh("scenes/fireplace_room/fireplace_room.obj", true);
	//scene.add_mesh("scenes/cbox/CornellBox-Original.obj", true);
	scene.build_rt();

	// setup the camera
	const vec3 look_from = scene.m_bbox_max * 1.1f;
	const vec3 look_at = (scene.m_bbox_max + scene.m_bbox_min) * 0.5f;
	const vec3 up = vec3(0.0f, 1.0f, 0.0f);
	FpsCamera camera(look_from,
					 look_at,
					 up,
					 glm::radians(45.0f),
					 static_cast<float>(Extent.width) / static_cast<float>(Extent.height));
	camera.m_move_speed = length(scene.m_bbox_max - scene.m_bbox_min) * 0.1f;
	camera.m_up_speed = length(scene.m_bbox_max - scene.m_bbox_min) * 0.1f;

	// run renderer
#if 1
	Rtao rtao;
	rtao.run(&scene, &camera);
#else
	PathTracer path_tracer;
	path_tracer.run(&scene, &camera);
#endif

	return 0;
}