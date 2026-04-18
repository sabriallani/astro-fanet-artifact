/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "astro-packet.h"
#include "ns3/log.h"

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("AstroPacket");

// ========================================================================
// AstroBeaconHeader
// ========================================================================
NS_OBJECT_ENSURE_REGISTERED (AstroBeaconHeader);

AstroBeaconHeader::AstroBeaconHeader ()
  : m_action (ACTION_FORWARD),
    m_nextHop (Ipv4Address::GetBroadcast ()),
    m_role (ROLE_RELAYING),
    m_suppressionState (0.0),
    m_nodeId (0),
    m_timestamp (Seconds (0)),
    m_posX (0), m_posY (0), m_posZ (0),
    m_velX (0), m_velY (0), m_velZ (0),
    m_energy (1.0),
    m_trustScore (1.0),
    m_compressedEmbedding (COMPRESSED_EMBED_DIM, 0.0f)
{
  m_hmac.fill (0);
}

AstroBeaconHeader::~AstroBeaconHeader ()
{
}

TypeId
AstroBeaconHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::astro::AstroBeaconHeader")
    .SetParent<Header> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<AstroBeaconHeader> ();
  return tid;
}

TypeId
AstroBeaconHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
AstroBeaconHeader::GetSerializedSize (void) const
{
  // action(1) + nextHop(4) + role(1) + suppression(8) + nodeId(4) + timestamp(8)
  // + pos(24) + vel(24) + energy(8) + trust(8) + embedding(COMPRESSED_EMBED_DIM*4) + hmac(32)
  return 1 + 4 + 1 + 8 + 4 + 8 + 24 + 24 + 8 + 8 + COMPRESSED_EMBED_DIM * 4 + HMAC_SIZE;
}

void
AstroBeaconHeader::Serialize (Buffer::Iterator start) const
{
  start.WriteU8 (static_cast<uint8_t> (m_action));
  start.WriteHtonU32 (m_nextHop.Get ());
  start.WriteU8 (static_cast<uint8_t> (m_role));

  // Write doubles as uint64 bit patterns
  uint64_t tmp;
  std::memcpy (&tmp, &m_suppressionState, sizeof (double));
  start.WriteHtonU64 (tmp);

  start.WriteHtonU32 (m_nodeId);
  start.WriteHtonU64 (m_timestamp.GetNanoSeconds ());

  double posVals[] = {m_posX, m_posY, m_posZ, m_velX, m_velY, m_velZ, m_energy, m_trustScore};
  for (int i = 0; i < 8; i++)
    {
      std::memcpy (&tmp, &posVals[i], sizeof (double));
      start.WriteHtonU64 (tmp);
    }

  // Compressed embedding
  for (uint32_t i = 0; i < COMPRESSED_EMBED_DIM; i++)
    {
      uint32_t ftmp;
      float val = (i < m_compressedEmbedding.size ()) ? m_compressedEmbedding[i] : 0.0f;
      std::memcpy (&ftmp, &val, sizeof (float));
      start.WriteHtonU32 (ftmp);
    }

  // HMAC
  for (uint32_t i = 0; i < HMAC_SIZE; i++)
    {
      start.WriteU8 (m_hmac[i]);
    }
}

uint32_t
AstroBeaconHeader::Deserialize (Buffer::Iterator start)
{
  m_action = static_cast<AstroAction> (start.ReadU8 ());
  m_nextHop = Ipv4Address (start.ReadNtohU32 ());
  m_role = static_cast<MissionRole> (start.ReadU8 ());

  uint64_t tmp = start.ReadNtohU64 ();
  std::memcpy (&m_suppressionState, &tmp, sizeof (double));

  m_nodeId = start.ReadNtohU32 ();
  m_timestamp = NanoSeconds (start.ReadNtohU64 ());

  double *posVals[] = {&m_posX, &m_posY, &m_posZ, &m_velX, &m_velY, &m_velZ, &m_energy, &m_trustScore};
  for (int i = 0; i < 8; i++)
    {
      tmp = start.ReadNtohU64 ();
      std::memcpy (posVals[i], &tmp, sizeof (double));
    }

  m_compressedEmbedding.resize (COMPRESSED_EMBED_DIM);
  for (uint32_t i = 0; i < COMPRESSED_EMBED_DIM; i++)
    {
      uint32_t ftmp = start.ReadNtohU32 ();
      std::memcpy (&m_compressedEmbedding[i], &ftmp, sizeof (float));
    }

  for (uint32_t i = 0; i < HMAC_SIZE; i++)
    {
      m_hmac[i] = start.ReadU8 ();
    }

  return GetSerializedSize ();
}

void
AstroBeaconHeader::Print (std::ostream &os) const
{
  os << "ASTRO-BEACON node=" << m_nodeId
     << " action=" << (int)m_action
     << " role=" << (int)m_role
     << " pos=(" << m_posX << "," << m_posY << "," << m_posZ << ")"
     << " energy=" << m_energy;
}

void
AstroBeaconHeader::SetCompressedEmbedding (const std::vector<float> &emb)
{
  m_compressedEmbedding.resize (COMPRESSED_EMBED_DIM);
  for (uint32_t i = 0; i < COMPRESSED_EMBED_DIM && i < emb.size (); i++)
    {
      m_compressedEmbedding[i] = emb[i];
    }
}

std::vector<float>
AstroBeaconHeader::GetCompressedEmbedding (void) const
{
  return m_compressedEmbedding;
}

// ========================================================================
// AstroDataHeader
// ========================================================================
NS_OBJECT_ENSURE_REGISTERED (AstroDataHeader);

AstroDataHeader::AstroDataHeader ()
  : m_trafficClass (SENSING),
    m_originId (0),
    m_seqNo (0),
    m_creationTime (Seconds (0)),
    m_hopCount (0),
    m_isBroadcast (false),
    m_bcastOrigX (0), m_bcastOrigY (0), m_bcastOrigZ (0),
    m_prevRelayX (0), m_prevRelayY (0), m_prevRelayZ (0)
{
}

AstroDataHeader::~AstroDataHeader ()
{
}

TypeId
AstroDataHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::astro::AstroDataHeader")
    .SetParent<Header> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<AstroDataHeader> ();
  return tid;
}

TypeId
AstroDataHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
AstroDataHeader::GetSerializedSize (void) const
{
  // tc(1) + originId(4) + seq(4) + time(8) + hops(1) + isBcast(1) + origPos(24) + prevRelayPos(24)
  return 1 + 4 + 4 + 8 + 1 + 1 + 24 + 24;
}

void
AstroDataHeader::Serialize (Buffer::Iterator start) const
{
  start.WriteU8 (static_cast<uint8_t> (m_trafficClass));
  start.WriteHtonU32 (m_originId);
  start.WriteHtonU32 (m_seqNo);
  start.WriteHtonU64 (m_creationTime.GetNanoSeconds ());
  start.WriteU8 (m_hopCount);
  start.WriteU8 (m_isBroadcast ? 1 : 0);

  uint64_t tmp;
  double vals[] = {m_bcastOrigX, m_bcastOrigY, m_bcastOrigZ,
                   m_prevRelayX, m_prevRelayY, m_prevRelayZ};
  for (int i = 0; i < 6; i++)
    {
      std::memcpy (&tmp, &vals[i], sizeof (double));
      start.WriteHtonU64 (tmp);
    }
}

uint32_t
AstroDataHeader::Deserialize (Buffer::Iterator start)
{
  m_trafficClass = static_cast<TrafficClass> (start.ReadU8 ());
  m_originId = start.ReadNtohU32 ();
  m_seqNo = start.ReadNtohU32 ();
  m_creationTime = NanoSeconds (start.ReadNtohU64 ());
  m_hopCount = start.ReadU8 ();
  m_isBroadcast = (start.ReadU8 () != 0);

  uint64_t tmp;
  double *vals[] = {&m_bcastOrigX, &m_bcastOrigY, &m_bcastOrigZ,
                    &m_prevRelayX, &m_prevRelayY, &m_prevRelayZ};
  for (int i = 0; i < 6; i++)
    {
      tmp = start.ReadNtohU64 ();
      std::memcpy (vals[i], &tmp, sizeof (double));
    }

  return GetSerializedSize ();
}

void
AstroDataHeader::Print (std::ostream &os) const
{
  os << "ASTRO-DATA class=" << (int)m_trafficClass
     << " origin=" << m_originId
     << " seq=" << m_seqNo
     << " hops=" << (int)m_hopCount
     << " bcast=" << m_isBroadcast;
}

} // namespace astro
} // namespace ns3
