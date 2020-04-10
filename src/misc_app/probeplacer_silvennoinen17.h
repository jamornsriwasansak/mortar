#include "misc_app/voxelizer.h"

const std::string DefaultDebugPathVoxelized = "_1_voxelized.obj";
const std::string DefaultDebugPathVoxelizedSolid = "_2_voxelized_solid.obj";
const std::string DefaultDebugPathNeighboursNonEmpty = "_3_non_empty_neighbours.obj";
const std::string DefaultDebugPathProbePositions = "_4_probe_positions.obj";
const int VoxelEmptyId = 0;
const int VoxelSurfaceId = 1;
const int VoxelSolidId = 2;
const int VoxelNonEmptyNeighbourId = 3;

struct ProbePlacerSilvennoinen17
{
	static constexpr int DefaultVoxelResolution = 15;
	static constexpr float DefaultVoxelExpandingFactor = 1.05f;

	struct Probe
	{
		vec3 m_position;
		float m_density;

		Probe(const vec3 & position):
			m_position(position),
			m_density(0.0f)
		{
		}
	};

	float
	estimate_rho_probe(const Scene & scene,
					   const size_t num_probes)
	{
		// find volume from w(idth) h(eight) d(epth)
		const vec3 whd = scene.m_bbox_max - scene.m_bbox_min;
		const float scene_volume = whd.x * whd.y * whd.z;
		
		// find approximate volume per probe
		const float probe_volume = scene_volume / static_cast<float>(num_probes);
		
		// since volume correspond to radius^3 
		const float radius = std::pow(probe_volume, 1.0f / 3.0f);

		// finally constant to scale radius. this magic number is obtained via eye balling.
		const float magic_number = 1.0f;

		return radius * magic_number;
	}

	VoxelizedScene
	get_voxelized_scene(const Scene & scene,
						const std::filesystem::path & debug_path = "")
	{
		Voxelizer voxelizer;
		VoxelizedScene voxelized_scene = voxelizer.run(scene,
													   DefaultVoxelResolution,
													   DefaultVoxelExpandingFactor,
													   true);
		if (debug_path != "")
		{
			std::string fullpath = debug_path.string() + std::string(DefaultDebugPathVoxelized);
			voxelized_scene.write_obj(fullpath);
		}
		return voxelized_scene;
	}

	// guess the empty space then solidify the rest of the voxels
	void
	make_solid_voxelization(VoxelizedScene * voxelized_scene,
							const std::filesystem::path & debug_path = "")
	{
		// find largest volume that we can floodfill
		int largest_volume = 0;
		int largest_volume_index = 0;
		int flood_fill_index_counter = -1;
		for (int z = 0; z < voxelized_scene->m_resolution.z; z++)
			for (int y = 0; y < voxelized_scene->m_resolution.y; y++)
				for (int x = 0; x < voxelized_scene->m_resolution.x; x++)
				{
					if ((*voxelized_scene)[ivec3(x, y, z)] == VoxelEmptyId)
					{
						int volume = voxelized_scene->flood_fill(ivec3(x, y, z),
																flood_fill_index_counter);
						if (volume > largest_volume)
						{
							largest_volume = volume;
							largest_volume_index = flood_fill_index_counter;
						}

						flood_fill_index_counter -= 1;
					}
				}

		// we assume the largest volume is empty space
		// mark empty space with 0, non empty space with 1
		for (int z = 0; z < voxelized_scene->m_resolution.z; z++)
			for (int y = 0; y < voxelized_scene->m_resolution.y; y++)
				for (int x = 0; x < voxelized_scene->m_resolution.x; x++)
				{
					if ((*voxelized_scene)[ivec3(x, y, z)] == largest_volume_index)
					{
						(*voxelized_scene)[ivec3(x, y, z)] = VoxelEmptyId;
					}
					else if ((*voxelized_scene)[ivec3(x, y, z)] != VoxelSurfaceId)
					{
						(*voxelized_scene)[ivec3(x, y, z)] = VoxelSolidId;
					}
				}

		if (debug_path != "")
		{
			std::string fullpath = debug_path.string() + std::string(DefaultDebugPathVoxelizedSolid);
			voxelized_scene->write_obj(fullpath);
		}
	}

	// mark which voxels are neighbours to non empty voxels
	void
	mark_surface_neighbours_voxels(VoxelizedScene * voxelized_scene,
								   const std::filesystem::path & debug_path = "")
	{
		for (int z = 0; z < voxelized_scene->m_resolution.z; z++)
			for (int y = 0; y < voxelized_scene->m_resolution.y; y++)
				for (int x = 0; x < voxelized_scene->m_resolution.x; x++)
				{
					if ((*voxelized_scene)[ivec3(x, y, z)] == 1)
					{
						for (int pz = -1; pz <= 1; pz++)
							for (int py = -1; py <= 1; py++)
								for (int px = -1; px <= 1; px++)
								{
									// ignore if px = 0, py = 0, pz = 0
									if (px == 0 && py == 0 && pz == 0)
									{
										continue;
									}

									// ignore if out of bounds
									ivec3 index(x + px, y + py, z + pz);
									if (any(greaterThanEqual(index, voxelized_scene->m_resolution)))
									{
										continue;
									}
									if (any(lessThan(index, ivec3(0))))
									{
										continue;
									}

									// ignore if voxel is non empty
									if ((*voxelized_scene)[index] != VoxelEmptyId)
									{
										continue;
									}

									// mark the voxel at index as neighbour
									(*voxelized_scene)[index] = VoxelNonEmptyNeighbourId;
								}
					}
				}

		if (debug_path != "")
		{
			std::string fullpath = debug_path.string() + std::string(DefaultDebugPathNeighboursNonEmpty);
			voxelized_scene->write_obj(fullpath, VoxelNonEmptyNeighbourId);
		}

	}

	std::vector<vec3>
	find_probes_locations(const Scene & scene,
						  const VoxelizedScene & voxelized_scene,
						  const size_t num_probes,
						  const std::filesystem::path & debug_path = "")
	{
		// list all candidate probes position
		std::vector<Probe> candidate_probes;
		for (int z = 0; z < voxelized_scene.m_resolution.z; z++)
			for (int y = 0; y < voxelized_scene.m_resolution.y; y++)
				for (int x = 0; x < voxelized_scene.m_resolution.x; x++)
				{
					if (voxelized_scene[ivec3(x, y, z)] == VoxelNonEmptyNeighbourId)
					{
						// get probe position
						const vec3 probe_position = voxelized_scene.get_position(ivec3(x, y, z));

						// if probe position is outside scene bound, ignore
						if (any(greaterThanEqual(probe_position, scene.m_bbox_max)))
						{
							continue;
						}
						if (any(lessThan(probe_position, scene.m_bbox_min)))
						{
							continue;
						}

						candidate_probes.emplace_back(probe_position);
					}
				}

		// initialize all candidate probe densities
		const float rho_probes = estimate_rho_probe(scene,
													num_probes);
		for (size_t i = 0; i < candidate_probes.size(); i++)
		{
			float probe_density = 0.0f;
			for (size_t j = 0; j < candidate_probes.size(); j++)
			{
				const vec3 & probe_i_position = candidate_probes[i].m_position;
				const vec3 & probe_j_position = candidate_probes[j].m_position;
				const float distance_i_j = distance(probe_i_position, probe_j_position);
				// compute weight
				const float density = probe_placement_kernel(distance_i_j,
															 rho_probes);
				probe_density += density;
			}
			candidate_probes[i].m_density = probe_density;
		}

		while (true)
		{
			if (candidate_probes.size() == num_probes)
			{
				break;
			}

			// find candidate with maximum density
			int i_max_density = -1;
			float max_density = 0.0f;
			for (size_t i = 0; i < candidate_probes.size(); i++)
			{
				if (candidate_probes[i].m_density > max_density)
				{
					i_max_density = static_cast<int>(i);
					max_density = candidate_probes[i].m_density;
				}
			}

			// pop the candidate with maximum density
			std::swap(candidate_probes[i_max_density], candidate_probes[candidate_probes.size() - 1]);
			Probe eliminated = candidate_probes[candidate_probes.size() - 1];
			candidate_probes.pop_back();

			// subtract the density of removed candidate probe
			for (size_t i = 0; i < candidate_probes.size(); i++)
			{
				const float distance_i_e = distance(eliminated.m_position, candidate_probes[i].m_position);
				const float density = probe_placement_kernel(distance_i_e,
															 rho_probes);
				candidate_probes[i].m_density -= density;
			}
		}

		if (debug_path != "")
		{
			std::string fullpath = debug_path.string() + std::string(DefaultDebugPathProbePositions);
			std::ofstream ofs(fullpath);
			for (const Probe & probe : candidate_probes)
			{
				ofs << "v " << probe.m_position.x << " " << probe.m_position.y << " " << probe.m_position.z << std::endl;
			}
		}

		std::vector<vec3> result(num_probes);
		for (size_t i = 0; i < num_probes; i++)
		{
			result[i] = candidate_probes[i].m_position;
		}
		return result;
	}

	float
	radius_search(const std::vector<vec3> & probe_positions,
				  const VoxelizedScene & voxelized_scene,
				  const size_t n_overlap)
	{
		// in the literature, they iterate through receiver 
		// but for simplicity, I decide to iterate through surface voxels instead

		float target_radius = 0.0f;

		// list all candidate probes position
		std::vector<float> dist2_surface_to_probes(probe_positions.size());
		for (int z = 0; z < voxelized_scene.m_resolution.z; z++)
			for (int y = 0; y < voxelized_scene.m_resolution.y; y++)
				for (int x = 0; x < voxelized_scene.m_resolution.x; x++)
				{
					ivec3 index(x, y, z);
					if (voxelized_scene[index] == VoxelSurfaceId)
					{
						const vec3 surface_position = voxelized_scene.get_position(index);
						for (size_t i = 0; i < probe_positions.size(); i++)
						{
							const float dist2_surface_to_probe = distance2(surface_position, probe_positions[i]);
							dist2_surface_to_probes[i] = dist2_surface_to_probe;
						}

						// find nth (order start from smallest to largest)
						std::nth_element(dist2_surface_to_probes.begin(),
										 dist2_surface_to_probes.begin() + n_overlap,
										 dist2_surface_to_probes.end(),
										 std::less<float>());
						const float nth_smallest_radius = dist2_surface_to_probes[n_overlap];
						target_radius = std::max(nth_smallest_radius, target_radius);
					}
				}

		target_radius += length(voxelized_scene.get_voxel_size()) * 0.5f;
		return target_radius;
	}

	struct ProbePlacementResult
	{
		std::vector<vec3> m_positions;
		float m_radius = 0.0f;
	};
	// num_probes = target number of probes in the scene
	// n_overlap = average overlapping probes per receiver
	ProbePlacementResult
	run(const Scene & scene,
		const size_t num_probes,
		size_t n_overlap = 10,
		std::filesystem::path debug_path = "")
	{
		// it is impossible of have 10 overlapping probes support if we limit number of probes to 2
		n_overlap = std::min(num_probes, n_overlap);

		// voxelize, make solid voxelization, mark neighbours of solid voxels
		VoxelizedScene voxelized_scene = get_voxelized_scene(scene,
															 debug_path);
		make_solid_voxelization(&voxelized_scene,
								debug_path);
		mark_surface_neighbours_voxels(&voxelized_scene,
									   debug_path);

		// based on voxel information, we extract probe position and do radius search
		ProbePlacementResult result;
		result.m_positions = find_probes_locations(scene,
												   voxelized_scene,
												   num_probes,
												   debug_path);
		result.m_radius = radius_search(result.m_positions,
										voxelized_scene,
										n_overlap);

		return result;
	}
};
