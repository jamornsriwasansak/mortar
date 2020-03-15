#pragma once

#include "common/mortar.h"
#include "common/scene.h"
#include "common/camera.h"
#include "common/stopwatch.h"
#include "common/bluenoise.h"

#include "gavulkan/appcore.h"
#include "gavulkan/image.h"
#include "gavulkan/buffer.h"
#include "gavulkan/shader.h"
#include "gavulkan/raytracingpipeline.h"
#include "gavulkan/graphicspipeline.h"

struct PrimitivePathTracer
{
	RayTracingPipeline
	create_primitive_path_tracer_pipeline(const Scene & scene)
	{
		// create rtao pipeline
		Shader raygen_shader("shaders/renderer/primpath/raytrace.rgen",
							 vk::ShaderStageFlagBits::eRaygenNV);
		Shader raychit_shader("shaders/renderer/primpath/raytrace.rchit",
							  vk::ShaderStageFlagBits::eClosestHitNV);
		raychit_shader.m_uniform_from_set_and_binding.at({ 1, 0 })->m_num_descriptors = scene.m_triangle_instances.size();
		raychit_shader.m_uniform_from_set_and_binding.at({ 2, 0 })->m_num_descriptors = scene.m_triangle_instances.size();
		raychit_shader.m_uniform_from_set_and_binding.at({ 3, 0 })->m_num_descriptors = scene.m_triangle_instances.size();
		raychit_shader.m_uniform_from_set_and_binding.at({ 4, 0 })->m_num_descriptors = scene.m_triangle_instances.size();
		raychit_shader.m_uniform_from_set_and_binding.at({ 5, 0 })->m_num_descriptors = 1;
		raychit_shader.m_uniform_from_set_and_binding.at({ 6, 0 })->m_num_descriptors = scene.m_images_cache->m_images.size();
		Shader rayahit_shader("shaders/renderer/primpath/raytrace.rahit",
							  vk::ShaderStageFlagBits::eAnyHitNV);
		rayahit_shader.m_uniform_from_set_and_binding.at({ 2, 0 })->m_num_descriptors = scene.m_triangle_instances.size();
		rayahit_shader.m_uniform_from_set_and_binding.at({ 3, 0 })->m_num_descriptors = scene.m_triangle_instances.size();
		rayahit_shader.m_uniform_from_set_and_binding.at({ 4, 0 })->m_num_descriptors = scene.m_triangle_instances.size();
		rayahit_shader.m_uniform_from_set_and_binding.at({ 5, 0 })->m_num_descriptors = 1;
		rayahit_shader.m_uniform_from_set_and_binding.at({ 6, 0 })->m_num_descriptors = scene.m_images_cache->m_images.size();
		Shader shadow_raymiss_shader("shaders/renderer/primpath/rayshadow.rmiss",
									 vk::ShaderStageFlagBits::eMissNV);
		Shader raymiss_shader("shaders/renderer/primpath/raytrace.rmiss",
							  vk::ShaderStageFlagBits::eMissNV);
		RayTracingPipeline rt_pipeline({ &raygen_shader,
										 &raychit_shader,
										 //&rayahit_shader,
										 &raymiss_shader,
										 &shadow_raymiss_shader });

		return std::move(rt_pipeline);
	}

	GraphicsPipeline
	create_final_pipeline(const std::vector<RgbaColorAttachment> & color_attachments,
						  const DepthAttachment & depth_attachment)
	{
		Shader final_vertex_shader("shaders/renderer/final/passthrough.vert",
								   vk::ShaderStageFlagBits::eVertex);
		Shader final_frag_shader("shaders/renderer/final/post.frag",
								 vk::ShaderStageFlagBits::eFragment);
		GraphicsPipeline final_pipeline({ &final_vertex_shader,
										  &final_frag_shader },
										color_attachments,
										depth_attachment);
		return std::move(final_pipeline);
	}

	void
	run(Scene * scene,
		FpsCamera * camera,
		const RgbaImage2d<float> & envmap,
		const int num_spp_per_frame = 1)
	{
		BlueNoiseRng rng;

		/*
		*  pipelines
		*/

		// create rtao pipeline
		RayTracingPipeline rt_pipeline = create_primitive_path_tracer_pipeline(*scene);

		// fetch image_views
		std::vector<const vk::ImageView *> image_views = raw_ptrs(Core::Inst().m_vk_swapchain_image_views);

		// create attachments for final pipeline
		RgbaColorAttachment color_attachment;
		color_attachment.init_present(image_views);
		DepthImage2d<float> depth_image(Extent.width,
										Extent.height);
		DepthAttachment depth_attachment(&depth_image.m_vk_image_view.get());

		// create final pipeline (for output rt image buffer to screen)
		GraphicsPipeline final_pipeline = create_final_pipeline({ color_attachment },
																depth_attachment);

		// create storage image for raytracing
		RgbaImage2d<float> storage(Extent.width,
								   Extent.height,
								   vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
								   true);
		storage.init_sampler();

		/*
		*	buffers and descriptor sets
		*/

		// create camera properties buffers
		std::vector<Buffer> cam_prop_buffers;
		for (size_t i = 0; i < Core::Inst().m_vk_swapchain_images.size(); i++)
		{
			cam_prop_buffers.push_back(Buffer(sizeof(CameraProperties),
											  vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
											  vk::BufferUsageFlagBits::eUniformBuffer));
		}

		std::vector<vk::DescriptorSet> rt_descriptor_sets_0 = rt_pipeline
			.build_descriptor_sets(0)
			.set_accel_struct(0, *scene->m_tlas.m_vk_accel_struct)
			.set_storage_image(1, storage)
			.set_uniform_buffer_chain(2, cam_prop_buffers)
			.set_sampler(3, envmap)
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_1 = rt_pipeline
			.build_descriptor_sets(1)
			.set_storage_buffers_array(0, scene->get_indices_arrays_storage())
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_2 = rt_pipeline
			.build_descriptor_sets(2)
			.set_storage_buffers_array(0, scene->get_position_u_storage())
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_3 = rt_pipeline
			.build_descriptor_sets(3)
			.set_storage_buffers_array(0, scene->get_normal_v_storage())
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_4 = rt_pipeline
			.build_descriptor_sets(4)
			.set_storage_buffers_array(0, scene->get_material_id_storage())
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_5 = rt_pipeline
			.build_descriptor_sets(5)
			.set_storage_buffer(0, *scene->get_materials_buffer_storage())
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_6 = rt_pipeline
			.build_descriptor_sets(6)
			.set_samplers(0, scene->m_images_cache->m_images)
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_7 = rt_pipeline
			.build_descriptor_sets(7)
			.set_storage_buffer(0, rng.m_sobol_sequence_buffer)
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_8 = rt_pipeline
			.build_descriptor_sets(8)
			.set_storage_buffer(0, rng.m_scrambling_tile_buffer)
			.build();

		std::vector<vk::DescriptorSet> rt_descriptor_sets_9 = rt_pipeline
			.build_descriptor_sets(9)
			.set_storage_buffer(0, rng.m_ranking_tile_buffer)
			.build();

		std::vector<vk::DescriptorSet> final_descriptor_sets_0 = final_pipeline
			.build_descriptor_sets(0)
			.set_sampler(0, storage)
			.build();

		/*
		*	command buffers
		*/

		auto & cmd_buffers = Core::Inst().create_command_buffers();
		// loop over all different swapchain image views
		for (size_t i = 0; i < Core::Inst().m_vk_swapchain_images.size(); i++)
		{
			vk::CommandBufferBeginInfo command_buffer_begin_info = {};
			{
				command_buffer_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
				command_buffer_begin_info.setPInheritanceInfo(nullptr);
			}

			auto & cmd_buffer = *cmd_buffers[i];

			// begin
			cmd_buffer.begin(command_buffer_begin_info);

			// ray tracing pipeline
			cmd_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingNV,
									*rt_pipeline.m_vk_pipeline);
			cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV,
										  *rt_pipeline.m_vk_pipeline_layout,
										  0,
										  {
											rt_descriptor_sets_0[i],
											rt_descriptor_sets_1[i],
											rt_descriptor_sets_2[i],
											rt_descriptor_sets_3[i],
											rt_descriptor_sets_4[i],
											rt_descriptor_sets_5[i],
											rt_descriptor_sets_6[i],
											rt_descriptor_sets_7[i],
											rt_descriptor_sets_8[i],
											rt_descriptor_sets_9[i],
										  },
										  { });
			cmd_buffer.traceRaysNV(rt_pipeline.sbt_vk_buffer(), rt_pipeline.m_raygen_offset,
								   rt_pipeline.sbt_vk_buffer(), rt_pipeline.m_miss_offset, rt_pipeline.m_stride,
								   rt_pipeline.sbt_vk_buffer(), rt_pipeline.m_hit_offset, rt_pipeline.m_stride,
								   rt_pipeline.sbt_vk_buffer(), 0, 0,
								   Extent.width, Extent.height, 1);

			// draw ray tracing result
			cmd_buffer.beginRenderPass(final_pipeline.get_render_pass_begin_info(i), vk::SubpassContents::eInline);
			cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *final_pipeline.m_vk_pipeline);
			cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
										  *final_pipeline.m_vk_pipeline_layout,
										  0,
										  { final_descriptor_sets_0[i] },
										  { });
			cmd_buffer.draw(3, 1, 0, 0);
			cmd_buffer.endRenderPass();

			// end
			cmd_buffer.end();
		}

		int frame_counter = 0;

		/*
		*	main loop
		*/

		StopWatch stopwatch;
		Core::Inst().loop(
			[&](const Core::LoopCallbackParameters & params)
			{
				// compute frame time
				double frame_time = stopwatch.timeNanoSec() * 1e-9;
				stopwatch.reset();

				// update camera
				camera->update(float(frame_time));
				CameraProperties cam_props = camera->get_camera_props();

				cam_prop_buffers[params.m_image_index].copy_from(&cam_props,
																 sizeof(cam_props));

				// create submit info
				vk::SubmitInfo submit_info = {};
				std::array<vk::PipelineStageFlags, 1> wait_stage_mask = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
				std::array<vk::CommandBuffer, 1> command_buffers = { *cmd_buffers[params.m_image_index] };
				{
					submit_info.setPWaitDstStageMask(wait_stage_mask.data());
					submit_info.setWaitSemaphoreCount(uint32_t(params.m_wait_semaphores.size()));
					submit_info.setPWaitSemaphores(params.m_wait_semaphores.data());
					submit_info.setCommandBufferCount(uint32_t(command_buffers.size()));
					submit_info.setPCommandBuffers(command_buffers.data());
					submit_info.setSignalSemaphoreCount(uint32_t(params.m_signal_semaphores.size()));
					submit_info.setPSignalSemaphores(params.m_signal_semaphores.data());
				}

				// graphics queue submit
				Core::Inst().m_vk_graphics_queue.submit(submit_info,
														params.m_inflight_fence);
			});
	}
};
