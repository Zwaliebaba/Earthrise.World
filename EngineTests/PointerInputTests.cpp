#include "pch.h"

#include "PointerInput.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EngineTests
{

namespace
{

// A queued message with no window, as Window::PumpMessages would show it.
MSG Message(UINT _message, WPARAM _wParam, LPARAM _lParam)
{
  MSG message{};
  message.message = _message;
  message.wParam = _wParam;
  message.lParam = _lParam;
  return message;
}

} // namespace

TEST_CLASS(PointerInputTests)
{
public:
  TEST_METHOD(FollowsThePointerInClientPixels)
  {
    Engine::PointerInput pointer;
    pointer.Observe(Message(WM_MOUSEMOVE, 0, MAKELPARAM(100, 50)));
    Assert::AreEqual(100, pointer.XPixels());
    Assert::AreEqual(50, pointer.YPixels());
    pointer.Observe(Message(WM_MOUSEMOVE, 0, MAKELPARAM(-5, -7)));
    Assert::AreEqual(-5, pointer.XPixels(), L"a captured drag reports positions left of the window as negative");
    Assert::AreEqual(-7, pointer.YPixels());
  }

  TEST_METHOD(KeepsAButtonDownUntilItIsReleased)
  {
    Engine::PointerInput pointer;
    pointer.Observe(Message(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 20)));
    Assert::IsTrue(pointer.IsDown(Engine::PointerInput::Button::Left));
    Assert::IsTrue(pointer.WasPressed(Engine::PointerInput::Button::Left));
    Assert::IsFalse(pointer.IsDown(Engine::PointerInput::Button::Right));
    Assert::AreEqual(10, pointer.XPixels());

    pointer.EndFrame();
    Assert::IsTrue(pointer.IsDown(Engine::PointerInput::Button::Left));
    Assert::IsFalse(pointer.WasPressed(Engine::PointerInput::Button::Left), L"a press lasts one frame");

    pointer.Observe(Message(WM_LBUTTONUP, 0, MAKELPARAM(12, 22)));
    Assert::IsFalse(pointer.IsDown(Engine::PointerInput::Button::Left));
  }

  TEST_METHOD(AddsUpTheWheelForOneFrame)
  {
    Engine::PointerInput pointer;
    pointer.Observe(Message(WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0));
    pointer.Observe(Message(WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA / 2), 0));
    Assert::AreEqual(1.5F, pointer.WheelNotches(), 0.0001F);
    pointer.EndFrame();
    Assert::AreEqual(0.0F, pointer.WheelNotches(), 0.0001F);
    pointer.Observe(Message(WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), 0));
    Assert::AreEqual(-1.0F, pointer.WheelNotches(), 0.0001F);
  }

  TEST_METHOD(IgnoresOtherMessages)
  {
    Engine::PointerInput pointer;
    pointer.Observe(Message(WM_KEYDOWN, VK_SPACE, 0));
    Assert::AreEqual(0, pointer.XPixels());
    Assert::IsFalse(pointer.IsDown(Engine::PointerInput::Button::Left));
    Assert::AreEqual(0.0F, pointer.WheelNotches(), 0.0001F);
  }
};

} // namespace EngineTests
