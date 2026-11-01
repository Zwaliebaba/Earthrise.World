#pragma once

#include <DirectXMath.h>

#include "KeyboardInput.h"
#include "OrthographicCamera.h"
#include "PointerInput.h"

#include <cstdint>

namespace Client
{

/// The player's camera (Hud.md §3 and §7; RendererLook.md §7): the orthographic camera at Client/Look.h's angles, steered
/// by Hud.md's global keys, and framing the area below the top bar, so that the point it looks at is drawn at the
/// middle of what the top bar leaves uncovered.
class Camera
{
public:
  /// Looks at _targetMeters from the default azimuth and view height.
  explicit Camera(const DirectX::XMFLOAT3& _targetMeters);

  /// Hud.md's global keys: WASD and the arrows or a middle-drag pan, the wheel and + and - zoom, and [ ] or Page Up and
  /// Page Down turn a quarter. The drag is in pixels of a target _heightPixels high. Then eases the turn for _seconds.
  void Steer(const Engine::KeyboardInput& _keyboard, const Engine::PointerInput& _pointer, std::int32_t _dragXPixels,
             std::int32_t _dragYPixels, std::uint32_t _heightPixels, float _seconds);

  /// The view-projection for a target _aspect wide over high and _heightPixels high, whose top _topBarPixels the top bar
  /// covers.
  [[nodiscard]] DirectX::XMMATRIX ViewProjection(float _aspect, std::uint32_t _heightPixels, float _topBarPixels) const noexcept;
  [[nodiscard]] DirectX::XMVECTOR Forward() const noexcept;
  [[nodiscard]] float ViewHeightMeters() const noexcept;
  [[nodiscard]] float AzimuthRadians() const noexcept;

private:
  Engine::OrthographicCamera m_camera;
};

} // namespace Client
