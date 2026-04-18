/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET: Adaptive 3D Broadcast Storm Mitigation (A3D-BSM)
 * Implements Section 3.3 (Layer 3)
 *
 * Key concepts:
 * - 3D Adaptive Suppression Zone: time-varying ellipsoid (Eq. 7-8)
 * - Semi-axes computed from SLM embedding via learned linear head W_z (Eq. 8)
 * - Thresholded rebroadcast rule combining zone membership, density, angular diversity (Eq. 10)
 * - Emergency packets are NEVER suppressed (Property 3)
 */
#ifndef A3D_BSM_H
#define A3D_BSM_H

#include "ns3/object.h"
#include "ns3/vector.h"
#include "astro-packet.h"
#include <vector>
#include <map>
#include <set>

namespace ns3 {
namespace astro {

// Broadcast event tracking
struct BroadcastEvent
{
  uint32_t originId;
  uint32_t seqNo;
  Vector3D originPosition;
  Time originTime;
  uint32_t rebroadcastCount;
};

/**
 * \brief Adaptive 3D Broadcast Storm Mitigation mechanism
 *
 * Extends the Zone-of-Relevance concept from DPMS (Allani et al., 2018)
 * to 3D aerial domain with learned, context-adaptive suppression zones.
 */
class A3dBsm : public Object
{
public:
  static TypeId GetTypeId (void);

  A3dBsm ();
  virtual ~A3dBsm ();

  /**
   * Compute the 3D adaptive suppression zone semi-axes from the SLM embedding.
   * Implements Eq. 8: [a(t), b(t), c(t)]^T = a_min + sigmoid(W_z * z_i(t)) * (a_max - a_min)
   *
   * \param embedding The SLM embedding z_i(t) of dimension d_z
   * \returns Vector3D containing (a, b, c) semi-axes
   */
  Vector3D ComputeSuppressionZone (const std::vector<float> &embedding) const;

  /**
   * Check if a position is inside the ellipsoidal suppression zone (Eq. 7).
   *
   * \param point Position to check
   * \param zoneCenter Center of the suppression zone (broadcast origin)
   * \param semiAxes Semi-axes (a, b, c) of the ellipsoid
   * \returns true if point is inside the zone
   */
  bool IsInsideZone (const Vector3D &point, const Vector3D &zoneCenter,
                     const Vector3D &semiAxes) const;

  /**
   * Compute angular diversity theta_i(t) between current node, broadcast origin,
   * and previous relay (Eq. 9).
   *
   * \param currentPos Current node position p_i
   * \param originPos Broadcast origin position p_0
   * \param prevRelayPos Previous relay position p_j
   * \returns Angle in radians [0, pi]
   */
  double ComputeAngularDiversity (const Vector3D &currentPos,
                                  const Vector3D &originPos,
                                  const Vector3D &prevRelayPos) const;

  /**
   * Compute suppression score sigma_i(t) (Eq. 9 in the paper, suppression decision).
   *
   * \param embedding SLM embedding z_i(t)
   * \param broadcastFeatures Broadcast packet features [priority, age, hop count, distance]
   * \param localDensity Local UAV density estimate rho_local(t)
   * \param mobilityGradient Local mobility gradient nabla v_i(t)
   * \returns Suppression score sigma_i(t)
   */
  double ComputeSuppressionScore (const std::vector<float> &embedding,
                                  const std::vector<double> &broadcastFeatures,
                                  double localDensity,
                                  const Vector3D &mobilityGradient) const;

  /**
   * Make the full rebroadcast decision (Eq. 10).
   * Implements the priority-aware, threshold-based suppression rule.
   *
   * \param trafficClass Traffic class of the broadcast packet
   * \param currentPos Current node position
   * \param originPos Broadcast origin position
   * \param prevRelayPos Previous relay position
   * \param embedding SLM embedding z_i(t)
   * \param localDensity Local density estimate
   * \param mobilityGradient Mobility gradient
   * \param broadcastFeatures Broadcast features
   * \returns ACTION_SUPPRESS if suppressed, ACTION_BROADCAST otherwise
   */
  AstroAction DecideRebroadcast (TrafficClass trafficClass,
                                  const Vector3D &currentPos,
                                  const Vector3D &originPos,
                                  const Vector3D &prevRelayPos,
                                  const std::vector<float> &embedding,
                                  double localDensity,
                                  const Vector3D &mobilityGradient,
                                  const std::vector<double> &broadcastFeatures);

  /**
   * Record a broadcast event for tracking redundancy.
   */
  void RecordBroadcast (uint32_t originId, uint32_t seqNo,
                        const Vector3D &originPos, Time originTime);

  /**
   * Check if a broadcast has already been seen.
   */
  bool HasSeenBroadcast (uint32_t originId, uint32_t seqNo) const;

  /**
   * Get the current broadcast redundancy ratio (BRR).
   */
  double GetBroadcastRedundancyRatio () const;

  // Parameters from the paper
  void SetRmax (double rmax) { m_Rmax = rmax; }
  void SetAmin (double amin) { m_aMin = amin; }
  void SetAmax (double amax) { m_aMax = amax; }
  void SetDensityThreshold (double rhoTh) { m_rhoTh = rhoTh; }
  void SetAngularThreshold (double thetaTh) { m_thetaTh = thetaTh; }
  void SetSuppressionThreshold (double sigmaTh) { m_sigmaTh = sigmaTh; }

  // Statistics
  uint32_t GetTotalBroadcasts () const { return m_totalBroadcasts; }
  uint32_t GetSuppressedBroadcasts () const { return m_suppressedBroadcasts; }

private:
  // Zone parameters
  double m_Rmax;      // Maximum communication range (400m from Table 2)
  double m_aMin;      // Minimum semi-axis = 0.2 * R_max
  double m_aMax;      // Maximum semi-axis = R_max

  // Thresholds (Eq. 10)
  double m_rhoTh;     // Density threshold rho_th
  double m_thetaTh;   // Angular threshold theta_th (radians)
  double m_sigmaTh;   // Learned suppression threshold sigma_thresh

  // Learned weights W_z (3 x d_z) for semi-axis computation
  std::vector<std::vector<float>> m_Wz;

  // Learned weights w_sigma for suppression scoring
  std::vector<float> m_wSigma;

  // Broadcast tracking
  std::map<std::pair<uint32_t, uint32_t>, BroadcastEvent> m_seenBroadcasts;
  uint32_t m_totalBroadcasts;
  uint32_t m_suppressedBroadcasts;

  // Sigmoid function
  double Sigmoid (double x) const;

  // Initialize learned weights (pseudo-random or from file)
  void InitializeWeights ();
};

} // namespace astro
} // namespace ns3

#endif /* A3D_BSM_H */
