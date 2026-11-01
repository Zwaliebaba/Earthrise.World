#include "pch.h"

#include "OrthographicCamera.h"

namespace Engine
{

namespace
{

constexpr float QUARTER_TURN_RADIANS = DirectX::XM_PIDIV2;

} // namespace

OrthographicCamera::OrthographicCamera(const Desc& _desc) noexcept
  : m_elevationRadians(_desc.elevationRadians),
    m_azimuthRadians(_desc.azimuthRadians),
    m_targetAzimuthRadians(_desc.azimuthRadians),
    m_turnRadiansPerSecond(_desc.rotationSeconds > 0.0F ? QUARTER_TURN_RADIANS / _desc.rotationSeconds : 0.0F),
    m_viewHeightMeters(std::clamp(_desc.viewHeightMeters, _desc.minViewHeightMeters, _desc.maxViewHeightMeters)),
    m_minViewHeightMeters(_desc.minViewHeightMeters),
    m_maxViewHeightMeters(_desc.maxViewHeightMeters),
    m_targetMeters(_desc.targetMeters)
{
}

void OrthographicCamera::Pan(float _rightMeters, float _upMeters) noexcept
{
  // On the ground the screen's right is (-cos a, 0, sin a), as XMMatrixLookToLH builds it from the
  // forward vector, and the screen's up is the horizontal forward, (-sin a, 0, -cos a).
  const float sine = std::sin(m_azimuthRadians);
  const float cosine = std::cos(m_azimuthRadians);
  m_targetMeters.x += -_rightMeters * cosine - _upMeters * sine;
  m_targetMeters.z += _rightMeters * sine - _upMeters * cosine;
}

void OrthographicCamera::Zoom(float _notches) noexcept
{
  m_viewHeightMeters = std::clamp(m_viewHeightMeters * std::pow(ZOOM_STEP, -_notches), m_minViewHeightMeters, m_maxViewHeightMeters);
}

void OrthographicCamera::Rotate(std::int32_t _quarterTurns) noexcept
{
  m_targetAzimuthRadians += static_cast<float>(_quarterTurns) * QUARTER_TURN_RADIANS;
  if (m_turnRadiansPerSecond == 0.0F)
  {
    m_azimuthRadians = m_targetAzimuthRadians;
  }
}

void OrthographicCamera::Update(float _seconds) noexcept
{
  const float remaining = m_targetAzimuthRadians - m_azimuthRadians;
  const float step = m_turnRadiansPerSecond * _seconds;
  m_azimuthRadians = std::abs(remaining) <= step ? m_targetAzimuthRadians : m_azimuthRadians + std::copysign(step, remaining);
}

DirectX::XMVECTOR OrthographicCamera::Forward() const noexcept
{
  // The camera stands at the azimuth and looks back across the target, down at the elevation.
  const float horizontal = std::cos(m_elevationRadians);
  return DirectX::XMVectorSet(-horizontal * std::sin(m_azimuthRadians), -std::sin(m_elevationRadians),
                              -horizontal * std::cos(m_azimuthRadians), 0.0F);
}

DirectX::XMMATRIX OrthographicCamera::ViewProjection(float _aspect) const noexcept
{
  const DirectX::XMVECTOR forward = Forward();
  const DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&m_targetMeters);
  const DirectX::XMVECTOR eye = DirectX::XMVectorSubtract(target, DirectX::XMVectorScale(forward, EYE_DISTANCE_METERS));
  const DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(eye, forward, DirectX::XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F));
  // Near and far swapped: depth 1 at the near plane, 0 at the far one.
  const DirectX::XMMATRIX projection =
    DirectX::XMMatrixOrthographicLH(m_viewHeightMeters * _aspect, m_viewHeightMeters, FAR_METERS, NEAR_METERS);
  return DirectX::XMMatrixMultiply(view, projection);
}

float OrthographicCamera::ViewHeightMeters() const noexcept
{
  return m_viewHeightMeters;
}

float OrthographicCamera::AzimuthRadians() const noexcept
{
  return m_azimuthRadians;
}

DirectX::XMFLOAT3 OrthographicCamera::TargetMeters() const noexcept
{
  return m_targetMeters;
}

void OrthographicCamera::SetTargetMeters(const DirectX::XMFLOAT3& _targetMeters) noexcept
{
  m_targetMeters = _targetMeters;
}

} // namespace Engine
