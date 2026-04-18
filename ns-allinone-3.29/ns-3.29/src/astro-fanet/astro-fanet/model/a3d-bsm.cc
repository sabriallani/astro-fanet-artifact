/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "a3d-bsm.h"
#include "ns3/log.h"
#include <cmath>
#include <random>
#include <algorithm>

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("A3dBsm");
NS_OBJECT_ENSURE_REGISTERED (A3dBsm);

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
    m_aMin (0.2 * 400.0),   // = 80m (Eq. 8)
    m_aMax (400.0),          // = R_max (Eq. 8)
    m_rhoTh (3.0),           // Density threshold
    m_thetaTh (M_PI / 4.0),  // 45 degrees angular threshold
    m_sigmaTh (0.5),          // Suppression score threshold
    m_totalBroadcasts (0),
    m_suppressedBroadcasts (0)
{
  InitializeWeights ();
}

A3dBsm::~A3dBsm ()
{
}

double
A3dBsm::Sigmoid (double x) const
{
  return 1.0 / (1.0 + std::exp (-x));
}

void
A3dBsm::InitializeWeights ()
{
  // Initialize W_z (3 x EMBEDDING_DIM) with pseudo-random values
  // In deployment, these would be loaded from trained model
  std::mt19937 rng (12345);
  std::normal_distribution<float> dist (0.0f, 0.1f);

  m_Wz.resize (3, std::vector<float>(EMBEDDING_DIM, 0.0f));
  for (uint32_t i = 0; i < 3; i++)
    for (uint32_t j = 0; j < EMBEDDING_DIM; j++)
      m_Wz[i][j] = dist (rng);

  // Initialize w_sigma for suppression scoring
  // Input: [embedding || broadcast_features || density || mobility_gradient]
  uint32_t sigmaInputDim = EMBEDDING_DIM + 4 + 1 + 3;  // 264
  m_wSigma.resize (sigmaInputDim, 0.0f);
  for (auto &w : m_wSigma)
    w = dist (rng);
}

Vector3D
A3dBsm::ComputeSuppressionZone (const std::vector<float> &embedding) const
{
  // Eq. 8: [a(t), b(t), c(t)]^T = a_min + sigmoid(W_z * z_i(t)) * (a_max - a_min)
  double axes[3] = {0.0, 0.0, 0.0};

  for (uint32_t i = 0; i < 3; i++)
    {
      double dot = 0.0;
      for (uint32_t j = 0; j < EMBEDDING_DIM && j < embedding.size (); j++)
        dot += m_Wz[i][j] * embedding[j];

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
  // Eq. 9 (suppression): sigma_i(t) = w_sigma^T [z_i(t) || f_B || rho_local || nabla v_i]
  std::vector<double> input;
  input.reserve (EMBEDDING_DIM + 4 + 1 + 3);

  // Embedding
  for (uint32_t i = 0; i < EMBEDDING_DIM && i < embedding.size (); i++)
    input.push_back (static_cast<double>(embedding[i]));
  while (input.size () < EMBEDDING_DIM)
    input.push_back (0.0);

  // Broadcast features: [priority, age, hop count, distance_to_origin]
  for (const auto &f : broadcastFeatures)
    input.push_back (f);
  while (input.size () < EMBEDDING_DIM + 4)
    input.push_back (0.0);

  // Local density
  input.push_back (localDensity);

  // Mobility gradient
  input.push_back (mobilityGradient.x);
  input.push_back (mobilityGradient.y);
  input.push_back (mobilityGradient.z);

  // Dot product with learned weights
  double score = 0.0;
  for (uint32_t i = 0; i < input.size () && i < m_wSigma.size (); i++)
    score += input[i] * m_wSigma[i];

  return score;
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

  // Compute suppression zone
  Vector3D semiAxes = ComputeSuppressionZone (embedding);

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
