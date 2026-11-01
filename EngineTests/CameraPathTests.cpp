#include "pch.h"

#include "CameraPath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EngineTests
{

namespace
{

constexpr std::uint32_t LEG_FRAMES = 900; // the measured path's 15 s at 60 frames a second
constexpr float TOLERANCE_METERS = 1.0e-3F;

// A 48 m square about (48, 6, 58): corners at x 24 and 72, z 34 and 82.
Engine::CameraPath MakePath()
{
  return Engine::CameraPath({.centerMeters = {48.0F, 6.0F, 58.0F}, .sideMeters = 48.0F, .legFrames = LEG_FRAMES});
}

void AssertAt(const Engine::CameraPath::Pose& _pose, float _xMeters, float _zMeters)
{
  Assert::AreEqual(_xMeters, _pose.targetMeters.x, TOLERANCE_METERS);
  Assert::AreEqual(6.0F, _pose.targetMeters.y, TOLERANCE_METERS);
  Assert::AreEqual(_zMeters, _pose.targetMeters.z, TOLERANCE_METERS);
}

} // namespace

TEST_CLASS(CameraPathTests)
{
public:
  TEST_METHOD(StartsAtTheSouthWestCornerWithoutATurn)
  {
    const Engine::CameraPath path = MakePath();
    const Engine::CameraPath::Pose start = path.At(0);
    AssertAt(start, 24.0F, 34.0F);
    Assert::AreEqual(std::uint32_t{0}, start.leg);
    Assert::IsFalse(start.turnStarts);
    Assert::AreEqual(std::uint32_t{3600}, path.FrameCount());
  }

  TEST_METHOD(WalksASideAtASteadySpeed)
  {
    const Engine::CameraPath path = MakePath();
    AssertAt(path.At(LEG_FRAMES / 4), 24.0F, 46.0F);
    AssertAt(path.At(LEG_FRAMES / 2), 24.0F, 58.0F);
    Assert::IsFalse(path.At(LEG_FRAMES / 2).turnStarts);
  }

  TEST_METHOD(GoesRoundClockwiseTurningAtEachCorner)
  {
    const Engine::CameraPath path = MakePath();
    const std::array<std::array<float, 2>, 3> corners = {{{24.0F, 82.0F}, {72.0F, 82.0F}, {72.0F, 34.0F}}};
    for (std::uint32_t leg = 1; leg < Engine::CameraPath::LEG_COUNT; ++leg)
    {
      const Engine::CameraPath::Pose pose = path.At(leg * LEG_FRAMES);
      AssertAt(pose, corners[leg - 1][0], corners[leg - 1][1]);
      Assert::AreEqual(leg, pose.leg);
      Assert::IsTrue(pose.turnStarts);
      Assert::IsFalse(path.At(leg * LEG_FRAMES + 1).turnStarts);
    }
    // The last frame is one step short of the start, on the south side heading west.
    AssertAt(path.At(path.FrameCount() - 1), 24.0F + 48.0F / LEG_FRAMES, 34.0F);
  }

  TEST_METHOD(RefusesFramesBeyondItsEndAndEmptyPaths)
  {
    const Engine::CameraPath path = MakePath();
    Assert::ExpectException<std::out_of_range>([&path] { (void)path.At(path.FrameCount()); });
    Assert::ExpectException<std::invalid_argument>(
      [] { Engine::CameraPath empty({.centerMeters = {0.0F, 0.0F, 0.0F}, .sideMeters = 10.0F, .legFrames = 0}); });
    Assert::ExpectException<std::invalid_argument>(
      [] { Engine::CameraPath flat({.centerMeters = {0.0F, 0.0F, 0.0F}, .sideMeters = 0.0F, .legFrames = 10}); });
  }
};

} // namespace EngineTests
