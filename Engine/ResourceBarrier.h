#pragma once

#include "framework.h"

#include <d3d12.h>

namespace Engine
{

/// A transition of every subresource of _resource from _before to _after, written by hand (R14: no
/// d3dx12.h). The union is filled by member, because designators cannot name an anonymous union's.
[[nodiscard]] inline D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* _resource, D3D12_RESOURCE_STATES _before,
                                                              D3D12_RESOURCE_STATES _after) noexcept
{
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = _resource;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = _before;
  barrier.Transition.StateAfter = _after;
  return barrier;
}

} // namespace Engine
