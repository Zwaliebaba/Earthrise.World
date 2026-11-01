#pragma once

#include "ServerDataFile.h"
#include "World.h"

#include <cstdint>

namespace SimulationTests
{

/// A small generated world for the tick's suites: bedrock 2 voxels deep under earth, its surface
/// flat at GROUND_VOXELS, on the baked server data.
struct FlatWorld
{
  static constexpr std::uint8_t EARTH = 2;
  static constexpr std::uint8_t BEDROCK = 5;
  static constexpr std::int32_t BEDROCK_VOXELS = 2;
  static constexpr std::int32_t GROUND_VOXELS = 8; // the first empty voxel: units stand at this height
  static constexpr std::uint64_t BUILD_ID = 0xb0116e57;

  [[nodiscard]] static Simulation::World::Desc Desc(std::int32_t _widthChunks = 2, std::int32_t _depthChunks = 2)
  {
    return {.serverData = &BakedServerData(),
            .buildId = BUILD_ID,
            .worldSeed = 1,
            .resourceSeed = 2,
            .matchSeed = 3,
            .generatorVersion = 0,
            .widthChunks = _widthChunks,
            .heightChunks = 2,
            .depthChunks = _depthChunks,
            .generate =
              [](Protocol::ChunkCoordinate _coordinate, Protocol::Chunk& _chunk)
            {
              if (_coordinate.yChunks != 0)
              {
                return;
              }
              for (std::int32_t y = 0; y < GROUND_VOXELS; ++y)
              {
                for (std::int32_t z = 0; z < Protocol::CHUNK_EDGE_VOXELS; ++z)
                {
                  for (std::int32_t x = 0; x < Protocol::CHUNK_EDGE_VOXELS; ++x)
                  {
                    _chunk.Set(x, y, z, y < BEDROCK_VOXELS ? BEDROCK : EARTH);
                  }
                }
              }
            },
            .surfaceVoxels = [](std::int32_t, std::int32_t) { return GROUND_VOXELS; }};
  }
};

} // namespace SimulationTests
