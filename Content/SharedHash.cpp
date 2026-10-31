#include "pch.h"

#include "SharedHash.h"

#include "SharedHashValue.h"

namespace Content
{

std::uint64_t SharedHash() noexcept
{
  return Generated::SHARED_HASH;
}

} // namespace Content
