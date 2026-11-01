#include "pch.h"

#include "Query.h"

namespace Protocol
{

namespace
{

constexpr std::uint16_t AIM_CHECK_TAG = 1;
constexpr std::uint16_t SECTION_INTEGRITY_TAG = 2;
constexpr std::uint16_t TILE_REACH_TAG = 3;

void WritePosition(VoxelPosition _position, Core::ByteWriter& _writer)
{
  _writer.WriteI32(_position.xVoxels);
  _writer.WriteI32(_position.yVoxels);
  _writer.WriteI32(_position.zVoxels);
}

VoxelPosition ReadPosition(Core::ByteReader& _reader)
{
  // Read in order: braced initialisation evaluates its elements left to right.
  return VoxelPosition{.xVoxels = _reader.ReadI32(), .yVoxels = _reader.ReadI32(), .zVoxels = _reader.ReadI32()};
}

void WriteBool(bool _value, Core::ByteWriter& _writer)
{
  _writer.WriteU8(_value ? std::uint8_t{1} : std::uint8_t{0});
}

bool ReadBool(Core::ByteReader& _reader)
{
  const std::uint8_t value = _reader.ReadU8();
  if (value > 1)
  {
    throw std::invalid_argument("a query or answer holds a flag that is neither 0 nor 1");
  }
  return value == 1;
}

template <typename Enum> Enum ReadEnum(Core::ByteReader& _reader, Enum _last)
{
  const std::uint8_t value = _reader.ReadU8();
  if (value > static_cast<std::uint8_t>(_last))
  {
    throw std::invalid_argument("a query or answer holds a value its field does not have");
  }
  return static_cast<Enum>(value);
}

void WriteHeader(std::uint16_t _tag, Core::ByteWriter& _writer)
{
  _writer.WriteU16(_tag);
  _writer.WriteU16(QUERY_RECORD_VERSION);
}

std::uint16_t ReadHeader(Core::ByteReader& _reader)
{
  const std::uint16_t tag = _reader.ReadU16();
  const std::uint16_t version = _reader.ReadU16();
  if (tag != AIM_CHECK_TAG && tag != SECTION_INTEGRITY_TAG && tag != TILE_REACH_TAG)
  {
    throw std::invalid_argument("a query or answer with a tag this build does not know");
  }
  if (version != QUERY_RECORD_VERSION)
  {
    throw std::invalid_argument("a query or answer at a record version this build does not read");
  }
  return tag;
}

void WriteBits(const std::array<std::uint8_t, TILE_REACH_BYTES>& _bits, Core::ByteWriter& _writer)
{
  _writer.WriteBytes(std::as_bytes(std::span(_bits)));
}

std::array<std::uint8_t, TILE_REACH_BYTES> ReadBits(Core::ByteReader& _reader)
{
  std::array<std::uint8_t, TILE_REACH_BYTES> bits{};
  const std::span<const std::byte> bytes = _reader.ReadBytes(TILE_REACH_BYTES);
  std::ranges::transform(bytes, bits.begin(), [](std::byte _byte) { return std::to_integer<std::uint8_t>(_byte); });
  return bits;
}

} // namespace

void Encode(const QueryRequest& _request, Core::ByteWriter& _writer)
{
  _writer.WriteU32(_request.id);
  if (const auto* aim = std::get_if<AimCheck>(&_request.query))
  {
    WriteHeader(AIM_CHECK_TAG, _writer);
    _writer.WriteU64(aim->engine);
    WritePosition(aim->targetVoxels, _writer);
  }
  else if (const auto* integrity = std::get_if<SectionIntegrity>(&_request.query))
  {
    WriteHeader(SECTION_INTEGRITY_TAG, _writer);
    _writer.WriteU64(integrity->structure);
  }
  else if (const auto* reach = std::get_if<TileReach>(&_request.query))
  {
    WriteHeader(TILE_REACH_TAG, _writer);
    _writer.WriteU8(static_cast<std::uint8_t>(reach->navigationClass));
    WritePosition(reach->startCellVoxels, _writer);
    _writer.WriteI32(reach->tileX);
    _writer.WriteI32(reach->tileZ);
  }
}

QueryRequest DecodeQueryRequest(Core::ByteReader& _reader)
{
  const std::uint32_t id = _reader.ReadU32();
  const std::uint16_t tag = ReadHeader(_reader);
  if (tag == AIM_CHECK_TAG)
  {
    const EntityId engine = _reader.ReadU64();
    return {.id = id, .query = AimCheck{.engine = engine, .targetVoxels = ReadPosition(_reader)}};
  }
  if (tag == SECTION_INTEGRITY_TAG)
  {
    return {.id = id, .query = SectionIntegrity{.structure = _reader.ReadU64()}};
  }
  const NavigationClass navigationClass = ReadEnum(_reader, NavigationClass::Machines);
  const VoxelPosition start = ReadPosition(_reader);
  const std::int32_t tileX = _reader.ReadI32();
  return {.id = id,
          .query = TileReach{.navigationClass = navigationClass, .startCellVoxels = start, .tileX = tileX, .tileZ = _reader.ReadI32()}};
}

void Encode(const Answer& _answer, Core::ByteWriter& _writer)
{
  _writer.WriteU32(_answer.queryId);
  _writer.WriteU32(_answer.tick);
  if (const auto* aim = std::get_if<AimReading>(&_answer.reading))
  {
    WriteHeader(AIM_CHECK_TAG, _writer);
    _writer.WriteU8(static_cast<std::uint8_t>(aim->verdict));
    _writer.WriteI32(aim->distanceMeters);
    WriteBool(aim->strikes, _writer);
    WritePosition(aim->strikeVoxels, _writer);
  }
  else if (const auto* integrity = std::get_if<IntegrityReading>(&_answer.reading))
  {
    WriteHeader(SECTION_INTEGRITY_TAG, _writer);
    _writer.WriteU64(integrity->structure);
    _writer.WriteU8(static_cast<std::uint8_t>(integrity->ground));
    WriteBool(integrity->canBeUndermined, _writer);
    _writer.WriteU8(integrity->holeWidthVoxels);
    _writer.WriteU8(integrity->holeHeightVoxels);
    _writer.WriteU8(integrity->holeFallWidthVoxels);
    WriteBool(integrity->hasOpening, _writer);
    _writer.WriteU8(integrity->openingFloorVoxels);
    _writer.WriteU8(static_cast<std::uint8_t>(integrity->openingFloor));
    _writer.WriteU8(integrity->openingClearVoxels);
    WriteBool(integrity->routeNow, _writer);
    _writer.WriteU8(integrity->gapRunVoxels);
    _writer.WriteU8(integrity->gapFallRunVoxels);
    WriteBool(integrity->falling, _writer);
    WritePosition(integrity->weakMinVoxels, _writer);
    WritePosition(integrity->weakMaxVoxels, _writer);
  }
  else if (const auto* reach = std::get_if<TileReachReading>(&_answer.reading))
  {
    WriteHeader(TILE_REACH_TAG, _writer);
    _writer.WriteI32(reach->tileX);
    _writer.WriteI32(reach->tileZ);
    WriteBits(reach->usable, _writer);
    WriteBits(reach->reached, _writer);
  }
}

Answer DecodeAnswer(Core::ByteReader& _reader)
{
  const std::uint32_t queryId = _reader.ReadU32();
  const std::uint32_t tick = _reader.ReadU32();
  const std::uint16_t tag = ReadHeader(_reader);
  if (tag == AIM_CHECK_TAG)
  {
    AimReading aim{};
    aim.verdict = ReadEnum(_reader, AimVerdict::OutOfRange);
    aim.distanceMeters = _reader.ReadI32();
    aim.strikes = ReadBool(_reader);
    aim.strikeVoxels = ReadPosition(_reader);
    return {.queryId = queryId, .tick = tick, .reading = aim};
  }
  if (tag == SECTION_INTEGRITY_TAG)
  {
    IntegrityReading integrity{};
    integrity.structure = _reader.ReadU64();
    integrity.ground = ReadEnum(_reader, GroundKind::Bedrock);
    integrity.canBeUndermined = ReadBool(_reader);
    integrity.holeWidthVoxels = _reader.ReadU8();
    integrity.holeHeightVoxels = _reader.ReadU8();
    integrity.holeFallWidthVoxels = _reader.ReadU8();
    integrity.hasOpening = ReadBool(_reader);
    integrity.openingFloorVoxels = _reader.ReadU8();
    integrity.openingFloor = ReadEnum(_reader, OpeningFloor::Stone);
    integrity.openingClearVoxels = _reader.ReadU8();
    integrity.routeNow = ReadBool(_reader);
    integrity.gapRunVoxels = _reader.ReadU8();
    integrity.gapFallRunVoxels = _reader.ReadU8();
    integrity.falling = ReadBool(_reader);
    integrity.weakMinVoxels = ReadPosition(_reader);
    integrity.weakMaxVoxels = ReadPosition(_reader);
    return {.queryId = queryId, .tick = tick, .reading = integrity};
  }
  TileReachReading reach{};
  reach.tileX = _reader.ReadI32();
  reach.tileZ = _reader.ReadI32();
  reach.usable = ReadBits(_reader);
  reach.reached = ReadBits(_reader);
  return {.queryId = queryId, .tick = tick, .reading = reach};
}

} // namespace Protocol
