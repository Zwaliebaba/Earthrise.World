#pragma once

#include <DirectXMath.h>

#include <cstdint>

namespace Engine
{

/// A scripted camera path for measurement (ADR-011, decision 10). The camera's target goes round a
/// square on the ground at a steady speed, one side per leg, and the camera turns a quarter
/// clockwise as each leg after the first begins, so the path looks at the middle from all four
/// sides. The square is walked clockwise seen from above, from its south-west corner northwards, so
/// the middle stays on the same side of the screen. It is stepped by frame, not by clock, so every
/// run sees the same views whatever the frame rate.
class CameraPath
{
public:
  static constexpr std::uint32_t LEG_COUNT = 4;

  struct Desc
  {
    DirectX::XMFLOAT3 centerMeters; // the square's center; its height is the target's
    float sideMeters;
    std::uint32_t legFrames; // the frames each side takes
  };

  /// Where the path is at one frame.
  struct Pose
  {
    DirectX::XMFLOAT3 targetMeters;
    std::uint32_t leg; // 0 to LEG_COUNT - 1
    bool turnStarts;   // a quarter turn clockwise starts at this frame
  };

  /// Throws std::invalid_argument for a side that is not positive or a leg of no frames.
  explicit CameraPath(const Desc& _desc);

  /// LEG_COUNT legs of the Desc's frames.
  [[nodiscard]] std::uint32_t FrameCount() const noexcept;

  /// Throws std::out_of_range at or beyond FrameCount().
  [[nodiscard]] Pose At(std::uint32_t _frame) const;

private:
  Desc m_desc;
};

} // namespace Engine
