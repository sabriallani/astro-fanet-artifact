/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET/A3D-BSM ns-3 packet definitions
 * Packet definitions for intent exchange and control signaling
 */
#ifndef ASTRO_PACKET_H
#define ASTRO_PACKET_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include <vector>
#include <array>

namespace ns3 {
namespace astro {

// --------------------------------------------------------------------------
// Traffic class enumeration matching the paper's 4 priority classes
// --------------------------------------------------------------------------
enum TrafficClass : uint8_t
{
  EMERGENCY  = 0,
  COMMAND    = 1,
  SENSING    = 2,
  TELEMETRY  = 3
};

// Packet sizes per class (bytes) from Table 2 of the paper
static const uint32_t PACKET_SIZES[] = {128, 256, 512, 64};

// Maximum tolerable delay per class (ms)
static const double MAX_DELAY_MS[] = {50.0, 200.0, 1000.0, 2000.0};

// Priority levels (higher = more urgent)
static const uint8_t PRIORITY_LEVELS[] = {4, 3, 2, 1};

// --------------------------------------------------------------------------
// ASTRO Action enumeration (Eq. 5 in the paper)
// --------------------------------------------------------------------------
enum AstroAction : uint8_t
{
  ACTION_FORWARD     = 0,
  ACTION_AGGREGATE   = 1,
  ACTION_SUPPRESS    = 2,
  ACTION_BROADCAST   = 3,
  ACTION_ROLE_SWITCH = 4
};

// --------------------------------------------------------------------------
// Mission roles
// --------------------------------------------------------------------------
enum MissionRole : uint8_t
{
  ROLE_SENSING     = 0,
  ROLE_RELAYING    = 1,
  ROLE_AGGREGATING = 2,
  ROLE_GATEWAY     = 3
};

// --------------------------------------------------------------------------
// Intent vector dimension used by the executable scaffold. The paper-level
// coordination overhead is 16 bytes of quantized intent plus a 32-byte HMAC.
// --------------------------------------------------------------------------
static const uint32_t INTENT_DIM = 16;
static const uint32_t EMBEDDING_DIM = 256;  // d_z from the paper
static const uint32_t COMPRESSED_EMBED_DIM = 16;  // legacy scaffold context field
static const uint32_t HMAC_SIZE = 32;  // SHA-256 HMAC
static const uint32_t INTENT_VECTOR_BYTES = 16;
static const uint32_t INTENT_OVERHEAD = INTENT_VECTOR_BYTES + HMAC_SIZE;  // 48 bytes

// --------------------------------------------------------------------------
// ASTRO Beacon Header: carries intent/security fields plus simulator state
// needed by the executable ns-3 scaffold.
// --------------------------------------------------------------------------
class AstroBeaconHeader : public Header
{
public:
  AstroBeaconHeader ();
  virtual ~AstroBeaconHeader ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;

  // Intent vector fields (Eq. 14)
  void SetAction (AstroAction action) { m_action = action; }
  AstroAction GetAction (void) const { return m_action; }

  void SetIntendedNextHop (Ipv4Address addr) { m_nextHop = addr; }
  Ipv4Address GetIntendedNextHop (void) const { return m_nextHop; }

  void SetIntendedRole (MissionRole role) { m_role = role; }
  MissionRole GetIntendedRole (void) const { return m_role; }

  void SetSuppressionState (double sigma) { m_suppressionState = sigma; }
  double GetSuppressionState (void) const { return m_suppressionState; }

  void SetNodeId (uint32_t id) { m_nodeId = id; }
  uint32_t GetNodeId (void) const { return m_nodeId; }

  void SetTimestamp (Time t) { m_timestamp = t; }
  Time GetTimestamp (void) const { return m_timestamp; }

  // Position for A3D-BSM zone evaluation
  void SetPosition (double x, double y, double z) { m_posX = x; m_posY = y; m_posZ = z; }
  double GetPosX (void) const { return m_posX; }
  double GetPosY (void) const { return m_posY; }
  double GetPosZ (void) const { return m_posZ; }

  // Velocity for mobility gradient
  void SetVelocity (double vx, double vy, double vz) { m_velX = vx; m_velY = vy; m_velZ = vz; }
  double GetVelX (void) const { return m_velX; }
  double GetVelY (void) const { return m_velY; }
  double GetVelZ (void) const { return m_velZ; }

  // Energy
  void SetEnergy (double e) { m_energy = e; }
  double GetEnergy (void) const { return m_energy; }

  // Trust score for this node (self-reported, verified by receiver)
  void SetTrustScore (double t) { m_trustScore = t; }
  double GetTrustScore (void) const { return m_trustScore; }

  // Legacy compressed context field retained for scaffold compatibility
  void SetCompressedEmbedding (const std::vector<float> &emb);
  std::vector<float> GetCompressedEmbedding (void) const;

  // HMAC (simplified: 32-byte hash)
  void SetHmac (const std::array<uint8_t, HMAC_SIZE> &hmac) { m_hmac = hmac; }
  std::array<uint8_t, HMAC_SIZE> GetHmac (void) const { return m_hmac; }

private:
  AstroAction m_action;
  Ipv4Address m_nextHop;
  MissionRole m_role;
  double m_suppressionState;
  uint32_t m_nodeId;
  Time m_timestamp;
  double m_posX, m_posY, m_posZ;
  double m_velX, m_velY, m_velZ;
  double m_energy;
  double m_trustScore;
  std::vector<float> m_compressedEmbedding;
  std::array<uint8_t, HMAC_SIZE> m_hmac;
};

// --------------------------------------------------------------------------
// ASTRO Data Header: tags data packets with traffic class and metadata
// --------------------------------------------------------------------------
class AstroDataHeader : public Header
{
public:
  AstroDataHeader ();
  virtual ~AstroDataHeader ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;

  void SetTrafficClass (TrafficClass tc) { m_trafficClass = tc; }
  TrafficClass GetTrafficClass (void) const { return m_trafficClass; }

  void SetOriginId (uint32_t id) { m_originId = id; }
  uint32_t GetOriginId (void) const { return m_originId; }

  void SetSequenceNumber (uint32_t seq) { m_seqNo = seq; }
  uint32_t GetSequenceNumber (void) const { return m_seqNo; }

  void SetCreationTime (Time t) { m_creationTime = t; }
  Time GetCreationTime (void) const { return m_creationTime; }

  void SetHopCount (uint8_t hops) { m_hopCount = hops; }
  uint8_t GetHopCount (void) const { return m_hopCount; }
  void IncrementHopCount () { m_hopCount++; }

  void SetIsBroadcast (bool b) { m_isBroadcast = b; }
  bool GetIsBroadcast (void) const { return m_isBroadcast; }

  // For broadcast tracking (A3D-BSM)
  void SetBroadcastOrigin (double x, double y, double z) { m_bcastOrigX = x; m_bcastOrigY = y; m_bcastOrigZ = z; }
  double GetBcastOrigX (void) const { return m_bcastOrigX; }
  double GetBcastOrigY (void) const { return m_bcastOrigY; }
  double GetBcastOrigZ (void) const { return m_bcastOrigZ; }

  void SetPreviousRelayPos (double x, double y, double z) { m_prevRelayX = x; m_prevRelayY = y; m_prevRelayZ = z; }
  double GetPrevRelayX (void) const { return m_prevRelayX; }
  double GetPrevRelayY (void) const { return m_prevRelayY; }
  double GetPrevRelayZ (void) const { return m_prevRelayZ; }

private:
  TrafficClass m_trafficClass;
  uint32_t m_originId;
  uint32_t m_seqNo;
  Time m_creationTime;
  uint8_t m_hopCount;
  bool m_isBroadcast;
  double m_bcastOrigX, m_bcastOrigY, m_bcastOrigZ;
  double m_prevRelayX, m_prevRelayY, m_prevRelayZ;
};

} // namespace astro
} // namespace ns3

#endif /* ASTRO_PACKET_H */
