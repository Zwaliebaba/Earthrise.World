#include "pch.h"

#include "Chunk.h"

// Headless measurements (MvpPlan.md, M1, spike question 6). For now it times filling one chunk;
// the support search and the navigation rebuild follow. Wall-clock time is fine here: the bench
// measures the simulation, it is not part of it.
int main()
{
  Protocol::Chunk chunk;
  const auto start = std::chrono::steady_clock::now();
  for (std::int32_t y = 0; y < Protocol::Chunk::EDGE_VOXELS; ++y)
  {
    for (std::int32_t z = 0; z < Protocol::Chunk::EDGE_VOXELS; ++z)
    {
      for (std::int32_t x = 0; x < Protocol::Chunk::EDGE_VOXELS; ++x)
      {
        chunk.Set(x, y, z, 4);
      }
    }
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
  std::println("SimulationBench: filled one chunk, {} voxels, in {} us", chunk.SolidCount(), elapsed.count());
  return chunk.SolidCount() == Protocol::Chunk::VOXEL_COUNT ? 0 : 1;
}
