/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET: Behavioral Trust Scoring (Layer 4)
 * Implements Section 3.4 (Secure and Resilient Coordination Signaling)
 *
 * - HMAC verification of intent vectors (Eq. 11)
 * - Exponential-moving-average trust scoring (Eq. 12)
 * - Consistency checking between declared intent and observed behavior (Eq. 13)
 */
#ifndef TRUST_MANAGER_H
#define TRUST_MANAGER_H

#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "astro-packet.h"
#include <map>
#include <array>
#include <vector>
#include <set>

namespace ns3 {
namespace astro {

struct NeighborIntentRecord
{
  AstroAction declaredAction;
  Ipv4Address declaredNextHop;
  MissionRole declaredRole;
  double declaredSuppression;
  Time timestamp;
  bool verified;
};

struct NeighborTrustState
{
  double trustScore;           // tau_ij(t), initialized to 1.0
  NeighborIntentRecord lastIntent;
  uint32_t totalObservations;
  uint32_t consistentCount;
  bool flaggedAsUntrusted;
  Time lastUpdate;
};

/**
 * \brief Trust manager implementing HMAC verification and behavioral trust scoring
 */
class TrustManager : public Object
{
public:
  static TypeId GetTypeId (void);

  TrustManager ();
  virtual ~TrustManager ();

  /**
   * Set the pre-shared HMAC key (K in Eq. 11).
   * In simulation, all honest agents share the same key.
   */
  void SetHmacKey (const std::vector<uint8_t> &key);

  /**
   * Compute HMAC for an intent vector (Eq. 11).
   * auth_i(t) = HMAC_K(iota_i(t) || t || id_i)
   */
  std::array<uint8_t, HMAC_SIZE> ComputeHmac (const AstroBeaconHeader &beacon) const;

  /**
   * Verify HMAC of a received beacon. Returns true if valid.
   */
  bool VerifyHmac (const AstroBeaconHeader &beacon) const;

  /**
   * Record a neighbor's declared intent from their beacon.
   */
  void RecordIntent (uint32_t neighborId, const AstroBeaconHeader &beacon);

  /**
   * Observe a neighbor's actual behavior and update trust score (Eq. 12-13).
   * Called when we can verify whether the neighbor acted consistently with their intent.
   * \param neighborId The neighbor's node ID
   * \param observedAction What the neighbor actually did
   * \param observedNextHop Where the neighbor actually forwarded (if applicable)
   */
  void ObserveBehavior (uint32_t neighborId, AstroAction observedAction,
                        Ipv4Address observedNextHop = Ipv4Address::GetBroadcast ());

  /**
   * Get current trust score for a neighbor.
   * Returns tau_ij(t) in [0, 1].
   */
  double GetTrustScore (uint32_t neighborId) const;

  /**
   * Check if a neighbor is flagged as untrusted (tau_ij < tau_min).
   */
  bool IsUntrusted (uint32_t neighborId) const;

  /**
   * Get the set of trusted neighbor IDs.
   */
  std::set<uint32_t> GetTrustedNeighbors () const;

  /**
   * Get the set of flagged (untrusted) neighbor IDs for the SLM prompt.
   */
  std::set<uint32_t> GetFlaggedNeighbors () const;

  /**
   * Get the last recorded intent for a neighbor.
   */
  bool GetLastIntent (uint32_t neighborId, NeighborIntentRecord &record) const;

  // Parameters
  void SetBeta (double beta) { m_beta = beta; }        // Smoothing parameter (default: 0.3)
  void SetTauMin (double tauMin) { m_tauMin = tauMin; } // Minimum trust threshold (default: 0.5)

  // Byzantine simulation support
  void SetByzantine (bool isByz) { m_isByzantine = isByz; }
  bool IsByzantine () const { return m_isByzantine; }
  void SetDropRate (double rate) { m_dropRate = rate; }  // For selective-dropping attack
  double GetDropRate () const { return m_dropRate; }

private:
  std::vector<uint8_t> m_hmacKey;
  std::map<uint32_t, NeighborTrustState> m_trustTable;
  double m_beta;     // EMA smoothing parameter
  double m_tauMin;   // Minimum trust threshold

  // Byzantine agent parameters
  bool m_isByzantine;
  double m_dropRate;

  // Simple hash function for HMAC simulation (not cryptographic)
  uint32_t SimpleHash (const uint8_t *data, uint32_t len, uint32_t seed) const;
};

} // namespace astro
} // namespace ns3

#endif /* TRUST_MANAGER_H */
