#pragma once

#include "common/mortar.h"

#include "gavulkan/buffer.h"

struct BlueNoiseRng
{
	static constexpr const char * SobolSequenceFilename = "sobol_sequence";
	static constexpr const char * ScramblingTileFilenamePrefix = "scrambling_tile_256spp";
	static constexpr const char * RankingTileFilenamePrefix = "ranking_tile_256spp";

	Buffer m_sobol_sequence_buffer;
	Buffer m_scrambling_tile_buffer;
	Buffer m_ranking_tile_buffer;

	BlueNoiseRng():
		m_sobol_sequence_buffer(load_sequence("resource/" + std::string(SobolSequenceFilename))),
		m_scrambling_tile_buffer(load_sequence("resource/" + std::string(ScramblingTileFilenamePrefix))),
		m_ranking_tile_buffer(load_sequence("resource/" + std::string(RankingTileFilenamePrefix)))
	{
	}

	Buffer
	load_sequence(const std::filesystem::path & filepath)
	{
		std::vector<uint8_t> sequence;
		std::ifstream ifs(filepath);
		THROW_ASSERT(ifs.is_open(),
					 "cannot open bluenoise sequence file : " + filepath.string());
		char num[10];
		while (ifs.good())
		{
			ifs.getline(num, 10, ',');
			sequence.push_back(std::atoi(num));
		}

		Buffer result(sequence,
					  vk::MemoryPropertyFlagBits::eDeviceLocal,
					  vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eRayTracingNV);
		return std::move(result);
	}
};