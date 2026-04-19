/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "a3d-bsm.h"
#include "ns3/log.h"
#include <cmath>
#include <algorithm>

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("A3dBsm");
NS_OBJECT_ENSURE_REGISTERED (A3dBsm);

namespace {

const double W_XI[3][5] = {
  {0.85, -0.30, 0.65, -0.70, -0.35},
  {0.85, -0.30, 0.65, -0.70, -0.35},
  {0.55, -0.20, 0.40, -0.55, -0.25}
};

const double B_XI[3] = {-0.35, -0.35, -0.65};
const double W_SIGMA[6] = {0.95, 0.35, 0.65, 0.80, 0.45, 0.55};
const double B_SIGMA = -1.45;
const double PRIORITY_MAX = 4.0;

} // anonymous namespace

TypeId
A3dBsm::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::astro::A3dBsm")
    .SetParent<Object> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<A3dBsm> ();
  return tid;
}

A3dBsm::A3dBsm ()
  : m_Rmax (400.0),         // Table 2
    m_aMin (0.25 * 400.0),  // kappa_min * R_max = 100m
    m_aMax (400.0),          // = R_max (Eq. 8)
    m_rhoRef (12.0),         // Trusted-neighbor normalization reference
    m_gRef (25.0),           // Mobility-gradient normalization reference (m/s)
    m_hMax (6.0),            // Hop-count normalization reference
    m_rhoTh (3.0),           // Trusted-neighbor threshold
    m_thetaTh (M_PI / 3.0),  // 60 degrees angular threshold
    m_sigmaTh (0.55),        // Suppression score threshold
    m_totalBroadcasts (0),
    m_suppressedBroadcasts (0)
{
}

A3dBsm::~A3dBsm ()
{
}

double
A3dBsm::Sigmoid (double x) const
{
  return 1.0 / (1.0 + std::exp (-x));
}

double
A3dBsm::Clamp01 (double value) const
{
  return std::max (0.0, std::min (1.0, value));
}

double
A3dBsm::Norm (const Vector3D &v) const
{
  return std::sqrt (v.x * v.x + v.y * v.y + v.z * v.z);
}

double
A3dBsm::EstimateChannelOccupancy (double localDensity) const
{
  // The demo artifact does not expose a PHY idle/busy trace at this layer.
  // Use a bounded local-load proxy so chi_i remains deterministic and auditable.
  return Clamp01 (localDensity / m_rhoRef);
}

std::array<double, 5>
A3dBsm::BuildContext (double localDensity,
                      const Vector3D &mobilityGradient,
                      const std::vector<double> &broadcastFeatures) const
{
  double priority = broadcastFeatures.size () > 0 ? broadcastFeatures[0] : 1.0;
  double hopCount = broadcastFeatures.size () > 2 ? broadcastFeatures[2] : 0.0;

  return {
    Clamp01 (localDensity / m_rhoRef),
    Clamp01 (Norm (mobilityGradient) / m_gRef),
    EstimateChannelOccupancy (localDensity),
    Clamp01 (priority / PRIORITY_MAX),
    Clamp01 (hopCount / m_hMax)
  };
}

Vector3D
A3dBsm::ComputeSuppressionZone (const std::vector<float> &embedding) const
{
  (void) embedding;
  std::vector<double> neutralFeatures = {1.0, 0.0, 0.0, 0.0};
  return ComputeSuppressionZone (0.0, Vector3D (0.0, 0.0, 0.0), neutralFeatures);
}

Vector3D
A3dBsm::ComputeSuppressionZone (double localDensity,
                                const Vector3D &mobilityGradient,
                                const std::vector<double> &broadcastFeatures) const
{
  // Paper appendix: [a,b,c]^T = a_min + sigmoid(W_xi * xi_i + b_xi) * (a_max - a_min)
  auto xi = BuildContext (localDensity, mobilityGradient, broadcastFeatures);
  double axes[3] = {0.0, 0.0, 0.0};

  for (uint32_t i = 0; i < 3; i++)
    {
      double dot = B_XI[i];
      for (uint32_t j = 0; j < xi.size (); j++)
        {
          dot += W_XI[i][j] * xi[j];
        }

      axes[i] = m_aMin + Sigmoid (dot) * (m_aMax - m_aMin);
    }

  return Vector3D (axes[0], axes[1], axes[2]);
}

bool
A3dBsm::IsInsideZone (const Vector3D &point, const Vector3D &zoneCenter,
                       const Vector3D &semiAxes) const
{
  // Eq. 7: (px - p0x)^2/a^2 + (py - p0y)^2/b^2 + (pz - p0z)^2/c^2 <= 1
  double dx = point.x - zoneCenter.x;
  double dy = point.y - zoneCenter.y;
  double dz = point.z - zoneCenter.z;

  if (semiAxes.x < 1e-6 || semiAxes.y < 1e-6 || semiAxes.z < 1e-6)
    return false;

  double val = (dx * dx) / (semiAxes.x * semiAxes.x) +
               (dy * dy) / (semiAxes.y * semiAxes.y) +
               (dz * dz) / (semiAxes.z * semiAxes.z);

  return val <= 1.0;
}

double
A3dBsm::ComputeAngularDiversity (const Vector3D &currentPos,
                                  const Vector3D &originPos,
                                  const Vector3D &prevRelayPos) const
{
  // Eq. 9: theta_i(t) = arccos(clip(dot(pi-p0, pj-p0) / (||pi-p0|| * ||pj-p0|| + eps)))
  // For origin-generated broadcasts (no previous relay), theta = pi

  const double eps = 1e-8;

  Vector3D vecI (currentPos.x - originPos.x,
                 currentPos.y - originPos.y,
                 currentPos.z - originPos.z);
  Vector3D vecJ (prevRelayPos.x - originPos.x,
                 prevRelayPos.y - originPos.y,
                 prevRelayPos.z - originPos.z);

  double normI = std::sqrt (vecI.x * vecI.x + vecI.y * vecI.y + vecI.z * vecI.z);
  double normJ = std::sqrt (vecJ.x * vecJ.x + vecJ.y * vecJ.y + vecJ.z * vecJ.z);

  if (normI < eps || normJ < eps)
    return M_PI;  // Origin-generated broadcast

  double dot = vecI.x * vecJ.x + vecI.y * vecJ.y + vecI.z * vecJ.z;
  double cosTheta = dot / (normI * normJ + eps);

  // Clip to [-1, 1]
  cosTheta = std::max (-1.0, std::min (1.0, cosTheta));

  return std::acos (cosTheta);
}

double
A3dBsm::ComputeSuppressionScore (const std::vector<float> &embedding,
                                  const std::vector<double> &broadcastFeatures,
                                  double localDensity,
                                  const Vector3D &mobilityGradient) const
{
  (void) embedding;
  return ComputeSuppressionScore (broadcastFeatures, localDensity, mobilityGradient);
}

double
A3dBsm::ComputeSuppressionScore (const std::vector<double> &broadcastFeatures,
                                  double localDensity,
                                  const Vector3D &mobilityGradient) const
{
  auto xi = BuildContext (localDensity, mobilityGradient, broadcastFeatures);
  double distance = broadcastFeatures.size () > 3 ? broadcastFeatures[3] : 0.0;
  double distanceNorm = Clamp01 (distance / m_Rmax);

  double psi[6] = {
    xi[0],
    1.0 - xi[1],
    xi[2],
    1.0 - xi[3],
    1.0 - xi[4],
    1.0 - distanceNorm
  };

  double score = B_SIGMA;
  for (uint32_t i = 0; i < 6; i++)
    {
      score += W_SIGMA[i] * psi[i];
    }

  return Sigmoid (score);
}

AstroAction
A3dBsm::DecideRebroadcast (TrafficClass trafficClass,
                            const Vector3D &currentPos,
                            const Vector3D &originPos,
                            const Vector3D &prevRelayPos,
                            const std::vector<float> &embedding,
                            double localDensity,
                            const Vector3D &mobilityGradient,
                            const std::vector<double> &broadcastFeatures)
{
  // Eq. 10: Priority-aware suppression rule
  m_totalBroadcasts++;

  // Rule 1: Emergency packets are NEVER suppressed (Property 3)
  if (trafficClass == EMERGENCY)
    {
      NS_LOG_DEBUG ("Emergency packet: always broadcast");
      return ACTION_BROADCAST;
    }

  // Compute suppression zone using the fixed offline-calibrated context head.
  Vector3D semiAxes = ComputeSuppressionZone (localDensity, mobilityGradient, broadcastFeatures);

  // Rule 2: If outside suppression zone, broadcast
  if (!IsInsideZone (currentPos, originPos, semiAxes))
    {
      NS_LOG_DEBUG ("Outside suppression zone: broadcast");
      return ACTION_BROADCAST;
    }

  // Compute angular diversity
  double theta = ComputeAngularDiversity (currentPos, originPos, prevRelayPos);

  // Compute suppression score
  double sigma = ComputeSuppressionScore (embedding, broadcastFeatures,
                                           localDensity, mobilityGradient);

  // Rule 3: Suppress if inside zone AND dense enough AND low angular diversity AND high score
  if (localDensity >= m_rhoTh && theta <= m_thetaTh && sigma > m_sigmaTh)
    {
      m_suppressedBroadcasts++;
      NS_LOG_DEBUG ("Suppressed: density=" << localDensity
                    << " theta=" << theta << " sigma=" << sigma);
      return ACTION_SUPPRESS;
    }

  // Rule 4: Otherwise broadcast
  return ACTION_BROADCAST;
}

void
A3dBsm::RecordBroadcast (uint32_t originId, uint32_t seqNo,
                          const Vector3D &originPos, Time originTime)
{
  auto key = std::make_pair (originId, seqNo);
  auto it = m_seenBroadcasts.find (key);
  if (it == m_seenBroadcasts.end ())
    {
      BroadcastEvent evt;
      evt.originId = originId;
      evt.seqNo = seqNo;
      evt.originPosition = originPos;
      evt.originTime = originTime;
      evt.rebroadcastCount = 1;
      m_seenBroadcasts[key] = evt;
    }
  else
    {
      it->second.rebroadcastCount++;
    }
}

bool
A3dBsm::HasSeenBroadcast (uint32_t originId, uint32_t seqNo) const
{
  return m_seenBroadcasts.find (std::make_pair (originId, seqNo)) != m_seenBroadcasts.end ();
}

double
A3dBsm::GetBroadcastRedundancyRatio () const
{
  if (m_seenBroadcasts.empty ())
    return 0.0;

  double totalRebroadcasts = 0.0;
  for (const auto &pair : m_seenBroadcasts)
    totalRebroadcasts += pair.second.rebroadcastCount;

  return totalRebroadcasts / m_seenBroadcasts.size ();
}

} // namespace astro
} // namespace ns3
