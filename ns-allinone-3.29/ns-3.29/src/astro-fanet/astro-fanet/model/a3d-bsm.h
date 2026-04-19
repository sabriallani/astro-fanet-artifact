/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET: Adaptive 3D Broadcast Storm Mitigation (A3D-BSM)
 * Implements the A3D-BSM suppression layer.
 *
 * Key concepts:
 * - 3D Adaptive Suppression Zone: time-varying ellipsoid (Eq. 7-8)
 * - Semi-axes computed from bounded local context with fixed calibrated coefficients
 * - Thresholded rebroadcast rule combining zone membership, density, angular diversity (Eq. 10)
 * - Emergency packets are NEVER suppressed (Property 3)
 */
#ifndef A3D_BSM_H
#define A3D_BSM_H

#include "ns3/object.h"
#include "ns3/vector.h"
#include "astro-packet.h"
#include <array>
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
 * to 3D aerial domain with fixed offline-calibrated suppression zones.
 */
class A3dBsm : public Object
{
public:
  static TypeId GetTypeId (void);

  A3dBsm ();
  virtual ~A3dBsm ();

  /**
   * Compute the 3D adaptive suppression zone semi-axes from local context.
   * Implements the paper's fixed calibrated linear head:
   * [a,b,c]^T = a_min + sigmoid(W_xi * xi_i + b_xi) * (a_max - a_min).
   *
   * \param localDensity Trusted-neighbor count rho_i(t)
   * \param mobilityGradient Velocity difference relative to trusted neighbors
   * \param broadcastFeatures [priority, age, hop count, distance_to_origin]
   * \returns Vector3D containing (a, b, c) semi-axes
   */
  Vector3D ComputeSuppressionZone (double localDensity,
                                   const Vector3D &mobilityGradient,
                                   const std::vector<double> &broadcastFeatures) const;

  /**
   * Compatibility wrapper retained for older callers. The current paper does
   * not use SLM embeddings for A3D-BSM calibration.
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
   * \param broadcastFeatures Broadcast packet features [priority, age, hop count, distance]
   * \param localDensity Trusted-neighbor count rho_i(t)
   * \param mobilityGradient Local mobility gradient nabla v_i(t)
   * \returns Suppression score sigma_i(t)
   */
  double ComputeSuppressionScore (const std::vector<double> &broadcastFeatures,
                                  double localDensity,
                                  const Vector3D &mobilityGradient) const;

  /**
   * Compatibility wrapper retained for older callers. The embedding argument is ignored.
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
   * \param embedding Legacy scaffold argument, ignored by A3D-BSM
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
  void SetDensityReference (double rhoRef) { m_rhoRef = rhoRef; }
  void SetMobilityGradientReference (double gRef) { m_gRef = gRef; }
  void SetHopReference (double hMax) { m_hMax = hMax; }
  void SetDensityThreshold (double rhoTh) { m_rhoTh = rhoTh; }
  void SetAngularThreshold (double thetaTh) { m_thetaTh = thetaTh; }
  void SetSuppressionThreshold (double sigmaTh) { m_sigmaTh = sigmaTh; }

  // Statistics
  uint32_t GetTotalBroadcasts () const { return m_totalBroadcasts; }
  uint32_t GetSuppressedBroadcasts () const { return m_suppressedBroadcasts; }

private:
  // Zone parameters
  double m_Rmax;      // Maximum communication range (400m from Table 2)
  double m_aMin;      // Minimum semi-axis = kappa_min * R_max
  double m_aMax;      // Maximum semi-axis = R_max
  double m_rhoRef;    // Neighbor-count normalization reference
  double m_gRef;      // Mobility-gradient normalization reference
  double m_hMax;      // Hop-count normalization reference

  // Thresholds (Eq. 10)
  double m_rhoTh;     // Trusted-neighbor threshold rho_th
  double m_thetaTh;   // Angular threshold theta_th (radians)
  double m_sigmaTh;   // Suppression threshold sigma_th

  // Broadcast tracking
  std::map<std::pair<uint32_t, uint32_t>, BroadcastEvent> m_seenBroadcasts;
  uint32_t m_totalBroadcasts;
  uint32_t m_suppressedBroadcasts;

  // Sigmoid function
  double Sigmoid (double x) const;
  double Clamp01 (double value) const;
  double Norm (const Vector3D &v) const;
  double EstimateChannelOccupancy (double localDensity) const;
  std::array<double, 5> BuildContext (double localDensity,
                                      const Vector3D &mobilityGradient,
                                      const std::vector<double> &broadcastFeatures) const;
};

} // namespace astro
} // namespace ns3

#endif /* A3D_BSM_H */
