#include "pch.h"

#include "KeyboardInput.h"
#include "OrthographicCamera.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EngineTests
{

namespace
{

constexpr float TOLERANCE = 1.0e-4F;

Engine::OrthographicCamera MakeCamera()
{
  return Engine::OrthographicCamera({.elevationRadians = DirectX::XMConvertToRadians(34.0F),
                                     .azimuthRadians = DirectX::XMConvertToRadians(-135.0F),
                                     .viewHeightMeters = 84.0F,
                                     .minViewHeightMeters = 12.0F,
                                     .maxViewHeightMeters = 260.0F,
                                     .rotationSeconds = 0.3F,
                                     .targetMeters = {48.0F, 6.0F, 48.0F}});
}

DirectX::XMFLOAT4 Project(const Engine::OrthographicCamera& _camera, float _x, float _y, float _z)
{
  DirectX::XMFLOAT4 clip{};
  DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(DirectX::XMVectorSet(_x, _y, _z, 1.0F), _camera.ViewProjection(16.0F / 9.0F)));
  return clip;
}

MSG KeyMessage(UINT _message, WPARAM _key, LPARAM _flags)
{
  MSG message{};
  message.message = _message;
  message.wParam = _key;
  message.lParam = _flags;
  return message;
}

} // namespace

TEST_CLASS(CameraTests)
{
public:
  TEST_METHOD(CentersItsTarget)
  {
    const Engine::OrthographicCamera camera = MakeCamera();
    const DirectX::XMFLOAT4 clip = Project(camera, 48.0F, 6.0F, 48.0F);
    Assert::AreEqual(0.0F, clip.x, TOLERANCE);
    Assert::AreEqual(0.0F, clip.y, TOLERANCE);
    Assert::IsTrue(clip.z > 0.0F && clip.z < 1.0F, L"the target lies between the near and far planes");
  }

  TEST_METHOD(GivesNearerPointsTheLargerDepth)
  {
    const Engine::OrthographicCamera camera = MakeCamera();
    DirectX::XMFLOAT3 towardsEye{};
    DirectX::XMStoreFloat3(&towardsEye, DirectX::XMVectorScale(camera.Forward(), -10.0F));
    const float nearer = Project(camera, 48.0F + towardsEye.x, 6.0F + towardsEye.y, 48.0F + towardsEye.z).z;
    Assert::IsTrue(nearer > Project(camera, 48.0F, 6.0F, 48.0F).z, L"reversed Z: nearer is larger");
  }

  TEST_METHOD(ShowsItsViewHeightOnScreen)
  {
    // Lifting a point by half the view height, divided by cos(elevation) to undo the tilt, takes it
    // to the top edge of the screen.
    const Engine::OrthographicCamera camera = MakeCamera();
    const float lift = 42.0F / std::cos(DirectX::XMConvertToRadians(34.0F));
    Assert::AreEqual(1.0F, Project(camera, 48.0F, 6.0F + lift, 48.0F).y, TOLERANCE);
  }

  TEST_METHOD(ClampsTheZoom)
  {
    Engine::OrthographicCamera camera = MakeCamera();
    camera.Zoom(1.0F);
    Assert::AreEqual(84.0F / Engine::OrthographicCamera::ZOOM_STEP, camera.ViewHeightMeters(), TOLERANCE);
    camera.Zoom(100.0F);
    Assert::AreEqual(12.0F, camera.ViewHeightMeters(), TOLERANCE);
    camera.Zoom(-100.0F);
    Assert::AreEqual(260.0F, camera.ViewHeightMeters(), TOLERANCE);
  }

  TEST_METHOD(EasesAQuarterTurn)
  {
    Engine::OrthographicCamera camera = MakeCamera();
    const float start = camera.AzimuthRadians();
    camera.Rotate(1);
    camera.Update(0.15F);
    Assert::AreEqual(start + DirectX::XM_PIDIV4, camera.AzimuthRadians(), TOLERANCE, L"halfway after half the time");
    camera.Update(1.0F);
    Assert::AreEqual(start + DirectX::XM_PIDIV2, camera.AzimuthRadians(), TOLERANCE, L"and no further");
  }

  TEST_METHOD(PansAlongTheScreen)
  {
    Engine::OrthographicCamera camera = MakeCamera();
    camera.Pan(5.0F, 0.0F);
    // The old target now sits 5 m left of the centre: -5 / half the view width, in clip space.
    const float halfWidth = 84.0F * (16.0F / 9.0F) / 2.0F;
    const DirectX::XMFLOAT4 clip = Project(camera, 48.0F, 6.0F, 48.0F);
    Assert::AreEqual(-5.0F / halfWidth, clip.x, TOLERANCE);
    Assert::AreEqual(0.0F, clip.y, TOLERANCE, L"panning right does not move it up or down");
  }

  TEST_METHOD(ReadsKeysWithoutRepeats)
  {
    Engine::KeyboardInput keyboard;
    keyboard.Observe(KeyMessage(WM_KEYDOWN, 'W', 0));
    Assert::IsTrue(keyboard.IsDown('W'));
    Assert::IsTrue(keyboard.WasPressed('W'));
    keyboard.EndFrame();
    keyboard.Observe(KeyMessage(WM_KEYDOWN, 'W', LPARAM{1} << 30)); // an auto-repeat
    Assert::IsFalse(keyboard.WasPressed('W'), L"an auto-repeat is not a press");
    keyboard.Observe(KeyMessage(WM_KEYUP, 'W', 0));
    Assert::IsFalse(keyboard.IsDown('W'));
    keyboard.Observe(KeyMessage(WM_KEYDOWN, VK_NEXT, 0));
    keyboard.ReleaseAll();
    Assert::IsFalse(keyboard.IsDown(VK_NEXT), L"focus loss releases every key");
  }
};

} // namespace EngineTests
