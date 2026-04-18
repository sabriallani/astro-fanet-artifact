/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "trust-manager.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <cstring>
#include <algorithm>

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("TrustManager");
NS_OBJECT_ENSURE_REGISTERED (TrustManager);

TypeId
TrustManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::astro::TrustManager")
    .SetParent<Object> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<TrustManager> ();
  return tid;
}

TrustManager::TrustManager ()
  : m_beta (0.3),
    m_tauMin (0.5),
    m_isByzantine (false),
    m_dropRate (0.0)
{
  // Default 128-bit HMAC key (pre-shared)
  m_hmacKey.resize (16, 0xAB);
}

TrustManager::~TrustManager ()
{
}

void
TrustManager::SetHmacKey (const std::vector<uint8_t> &key)
{
  m_hmacKey = key;
}

uint32_t
TrustManager::SimpleHash (const uint8_t *data, uint32_t len, uint32_t seed) const
{
  // MurmurHash3-like hash for simulation purposes
  // NOT cryptographic — simulates HMAC overhead without real crypto
  uint32_t h = seed;
  for (uint32_t i = 0; i < len; i++)
    {
      h ^= data[i];
      h *= 0x5bd1e995;
      h ^= h >> 15;
    }
  return h;
}

std::array<uint8_t, HMAC_SIZE>
TrustManager::ComputeHmac (const AstroBeaconHeader &beacon) const
{
  // Construct message: iota_i(t) || t || id_i
  std::vector<uint8_t> message;

  // Intent action
  message.push_back (static_cast<uint8_t>(beacon.GetAction ()));

  // Next hop address (4 bytes)
  uint32_t addr = beacon.GetIntendedNextHop ().Get ();
  message.push_back ((addr >> 24) & 0xFF);
  message.push_back ((addr >> 16) & 0xFF);
  message.push_back ((addr >> 8) & 0xFF);
  message.push_back (addr & 0xFF);

  // Role
  message.push_back (static_cast<uint8_t>(beacon.GetIntendedRole ()));

  // Timestamp (8 bytes)
  uint64_t ts = beacon.GetTimestamp ().GetNanoSeconds ();
  for (int i = 7; i >= 0; i--)
    message.push_back ((ts >> (i * 8)) & 0xFF);

  // Node ID (4 bytes)
  uint32_t nodeId = beacon.GetNodeId ();
  for (int i = 3; i >= 0; i--)
    message.push_back ((nodeId >> (i * 8)) & 0xFF);

  // Append key
  message.insert (message.end (), m_hmacKey.begin (), m_hmacKey.end ());

  // Compute 32-byte hash (8 iterations of 4-byte hash with different seeds)
  std::array<uint8_t, HMAC_SIZE> hmac;
  for (uint32_t block = 0; block < 8; block++)
    {
      uint32_t h = SimpleHash (message.data (), message.size (), block * 7919 + 1);
      hmac[block * 4 + 0] = (h >> 24) & 0xFF;
      hmac[block * 4 + 1] = (h >> 16) & 0xFF;
      hmac[block * 4 + 2] = (h >> 8) & 0xFF;
      hmac[block * 4 + 3] = h & 0xFF;
    }

  return hmac;
}

bool
TrustManager::VerifyHmac (const AstroBeaconHeader &beacon) const
{
  auto expected = ComputeHmac (beacon);
  auto received = beacon.GetHmac ();
  return expected == received;
}

void
TrustManager::RecordIntent (uint32_t neighborId, const AstroBeaconHeader &beacon)
{
  NeighborIntentRecord record;
  record.declaredAction = beacon.GetAction ();
  record.declaredNextHop = beacon.GetIntendedNextHop ();
  record.declaredRole = beacon.GetIntendedRole ();
  record.declaredSuppression = beacon.GetSuppressionState ();
  record.timestamp = beacon.GetTimestamp ();
  record.verified = false;

  if (m_trustTable.find (neighborId) == m_trustTable.end ())
    {
      // Initialize new neighbor with trust = 1.0
      NeighborTrustState state;
      state.trustScore = 1.0;
      state.lastIntent = record;
      state.totalObservations = 0;
      state.consistentCount = 0;
      state.flaggedAsUntrusted = false;
      state.lastUpdate = Simulator::Now ();
      m_trustTable[neighborId] = state;
    }
  else
    {
      m_trustTable[neighborId].lastIntent = record;
      m_trustTable[neighborId].lastUpdate = Simulator::Now ();
    }
}

void
TrustManager::ObserveBehavior (uint32_t neighborId, AstroAction observedAction,
                                Ipv4Address observedNextHop)
{
  auto it = m_trustTable.find (neighborId);
  if (it == m_trustTable.end ())
    return;

  auto &state = it->second;
  state.totalObservations++;

  // Compute consistency score c_ij(t) (Eq. 13)
  double consistency = 0.0;
  if (observedAction == state.lastIntent.declaredAction)
    {
      consistency = 1.0;
      // If action is Forward, also check the next hop
      if (observedAction == ACTION_FORWARD &&
          observedNextHop != Ipv4Address::GetBroadcast () &&
          observedNextHop != state.lastIntent.declaredNextHop)
        {
          consistency = 0.5;  // Partial consistency
        }
      state.consistentCount++;
    }

  // Update trust score via EMA (Eq. 12)
  // tau_ij(t) = (1 - beta) * tau_ij(t-1) + beta * c_ij(t)
  state.trustScore = (1.0 - m_beta) * state.trustScore + m_beta * consistency;

  // Check threshold
  state.flaggedAsUntrusted = (state.trustScore < m_tauMin);

  if (state.flaggedAsUntrusted)
    {
      NS_LOG_INFO ("Node " << neighborId << " flagged as untrusted: trust="
                   << state.trustScore << " < " << m_tauMin);
    }

  state.lastIntent.verified = true;
}

double
TrustManager::GetTrustScore (uint32_t neighborId) const
{
  auto it = m_trustTable.find (neighborId);
  if (it == m_trustTable.end ())
    return 1.0;  // Unknown neighbor assumed trusted
  return it->second.trustScore;
}

bool
TrustManager::IsUntrusted (uint32_t neighborId) const
{
  auto it = m_trustTable.find (neighborId);
  if (it == m_trustTable.end ())
    return false;
  return it->second.flaggedAsUntrusted;
}

std::set<uint32_t>
TrustManager::GetTrustedNeighbors () const
{
  std::set<uint32_t> trusted;
  for (const auto &pair : m_trustTable)
    {
      if (!pair.second.flaggedAsUntrusted)
        trusted.insert (pair.first);
    }
  return trusted;
}

std::set<uint32_t>
TrustManager::GetFlaggedNeighbors () const
{
  std::set<uint32_t> flagged;
  for (const auto &pair : m_trustTable)
    {
      if (pair.second.flaggedAsUntrusted)
        flagged.insert (pair.first);
    }
  return flagged;
}

bool
TrustManager::GetLastIntent (uint32_t neighborId, NeighborIntentRecord &record) const
{
  auto it = m_trustTable.find (neighborId);
  if (it == m_trustTable.end ())
    return false;
  record = it->second.lastIntent;
  return true;
}

} // namespace astro
} // namespace ns3
