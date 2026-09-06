/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Executable broadcast-suppression baselines for the A3D-BSM artifact.
 * See broadcast-baselines.h for the rationale and the documented deviations.
 */
#include "broadcast-baselines.h"

#include "ns3/log.h"

#include <cmath>
#include <string>

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("BroadcastBaselines");

BroadcastBaseline
BaselineFromString (const std::string &name)
{
  if (name == "sf")  return BASELINE_SIMPLE_FLOODING;
  if (name == "pr")  return BASELINE_PROBABILISTIC;
  if (name == "cb")  return BASELINE_COUNTER_BASED;
  if (name == "sba") return BASELINE_SBA;
  return BASELINE_NONE;
}

std::string
BaselineToString (BroadcastBaseline mode)
{
  switch (mode)
    {
    case BASELINE_SIMPLE_FLOODING: return "sf";
    case BASELINE_PROBABILISTIC:   return "pr";
    case BASELINE_COUNTER_BASED:   return "cb";
    case BASELINE_SBA:             return "sba";
    case BASELINE_NONE:
    default:                       return "astro";
    }
}

bool
BaselineUsesRad (BroadcastBaseline mode)
{
  return mode == BASELINE_COUNTER_BASED || mode == BASELINE_SBA;
}

BroadcastBaselineEngine::BroadcastBaselineEngine ()
  : m_mode (BASELINE_NONE),
    m_rebroadcastProbability (0.6),
    m_counterThreshold (3),
    m_maxRad (MilliSeconds (10)),
    m_commRange (400.0)
{
  m_uniform = CreateObject<UniformRandomVariable> ();
}

void
BroadcastBaselineEngine::AssignStreams (int64_t stream)
{
  // Deterministic per-run reproducibility: the PR draws and the RAD samples
  // must depend only on the run seed and the node's stream offset.
  m_uniform->SetStream (stream);
}

Time
BroadcastBaselineEngine::DrawRad ()
{
  double maxMs = m_maxRad.GetMilliSeconds ();
  if (maxMs <= 0.0)
    {
      return Time (0);
    }
  return MilliSeconds (static_cast<int64_t> (m_uniform->GetValue (0.0, maxMs)));
}

void
BroadcastBaselineEngine::RecordReception (uint32_t originId, uint32_t sequenceNumber)
{
  m_duplicateCounts[std::make_pair (originId, sequenceNumber)]++;
}

uint32_t
BroadcastBaselineEngine::GetDuplicateCount (uint32_t originId,
                                            uint32_t sequenceNumber) const
{
  auto it = m_duplicateCounts.find (std::make_pair (originId, sequenceNumber));
  return it == m_duplicateCounts.end () ? 0 : it->second;
}

void
BroadcastBaselineEngine::ForgetPacket (uint32_t originId, uint32_t sequenceNumber)
{
  m_duplicateCounts.erase (std::make_pair (originId, sequenceNumber));
}

bool
BroadcastBaselineEngine::DecideImmediate ()
{
  switch (m_mode)
    {
    case BASELINE_SIMPLE_FLOODING:
      // SF: every node rebroadcasts every newly seen packet exactly once.
      // Duplicate suppression is handled upstream by the seen-packet set.
      return true;

    case BASELINE_PROBABILISTIC:
      // PR: independent Bernoulli trial per newly seen packet.
      return m_uniform->GetValue (0.0, 1.0) < m_rebroadcastProbability;

    default:
      NS_LOG_WARN ("DecideImmediate called for a deferred or inactive policy");
      return false;
    }
}

bool
BroadcastBaselineEngine::DecideAfterRad (uint32_t originId,
                                         uint32_t sequenceNumber,
                                         uint32_t additionalCover)
{
  uint32_t duplicateCount = GetDuplicateCount (originId, sequenceNumber);

  switch (m_mode)
    {
    case BASELINE_COUNTER_BASED:
      // CB: suppress once at least C copies have been overheard during the RAD.
      return duplicateCount < m_counterThreshold;

    case BASELINE_SBA:
      // SBA: rebroadcast only if this node reaches neighbours the previous
      // relay could not.  A node that adds no coverage stays silent.
      return additionalCover > 0;

    default:
      NS_LOG_WARN ("DecideAfterRad called for an immediate or inactive policy");
      return false;
    }
}

} // namespace astro
} // namespace ns3
