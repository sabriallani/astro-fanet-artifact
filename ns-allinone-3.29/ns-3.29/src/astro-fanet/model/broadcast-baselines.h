/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Executable broadcast-suppression baselines for the A3D-BSM artifact.
 *
 * Rationale
 * ---------
 * The manuscript positions A3D-BSM against classical broadcast-suppression
 * families.  Before this module the scenario only exposed `astro`, `aodv`,
 * `olsr`, `epidemic` and `dqn`, where `epidemic` and `dqn` were AODV with
 * tweaked flags.  Those are unicast routing protocols, not suppression
 * schemes, so they cannot answer the question the paper actually asks:
 * does the A3D-BSM decision rule suppress better than the standard
 * alternatives, under the same traffic, mobility and radio conditions?
 *
 * This module implements four real suppression policies that plug into the
 * exact same forwarding path as A3D-BSM, so a protocol switch changes the
 * decision rule and nothing else.
 *
 *   SF   Simple Flooding      -- rebroadcast every newly seen packet once.
 *   PR   Probabilistic        -- rebroadcast with fixed probability p.
 *   CB   Counter-Based        -- defer by a random assessment delay (RAD),
 *                               then rebroadcast only if fewer than C copies
 *                               of the packet were overheard meanwhile.
 *   SBA  Scalable Broadcast   -- defer by a RAD, then rebroadcast only if the
 *                               node covers neighbours the previous relay
 *                               could not reach.
 *
 * Deviations from the original publications are documented per policy and
 * mirrored in paper-reproduction/baseline_implementation_notes.md.  They are
 * approximations of the published rules, re-implemented here; they are not
 * the authors' original code and must not be described as such.
 */
#ifndef BROADCAST_BASELINES_H
#define BROADCAST_BASELINES_H

#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/vector.h"

#include <cstdint>
#include <map>
#include <utility>

namespace ns3 {
namespace astro {

/**
 * Suppression policy selector.  BASELINE_NONE means the A3D-BSM decision
 * rule is in charge (the contribution under evaluation).
 */
enum BroadcastBaseline
{
  BASELINE_NONE = 0,
  BASELINE_SIMPLE_FLOODING,
  BASELINE_PROBABILISTIC,
  BASELINE_COUNTER_BASED,
  BASELINE_SBA
};

/** Map a scenario protocol string to a baseline mode. */
BroadcastBaseline BaselineFromString (const std::string &name);

/** Human-readable name, used in logs and CSV provenance fields. */
std::string BaselineToString (BroadcastBaseline mode);

/** True when the policy defers its decision by a random assessment delay. */
bool BaselineUsesRad (BroadcastBaseline mode);

/**
 * \brief Decision engine for the classical broadcast-suppression baselines.
 *
 * One instance lives per node, inside AstroRoutingProtocol.  It holds only
 * the per-policy state the published rules require.
 */
class BroadcastBaselineEngine
{
public:
  BroadcastBaselineEngine ();

  void SetMode (BroadcastBaseline mode) { m_mode = mode; }
  BroadcastBaseline GetMode () const { return m_mode; }
  bool IsActive () const { return m_mode != BASELINE_NONE; }

  /** Rebroadcast probability for PR.  Default 0.6 (Ni et al. mid-range). */
  void SetRebroadcastProbability (double p) { m_rebroadcastProbability = p; }
  double GetRebroadcastProbability () const { return m_rebroadcastProbability; }

  /** Duplicate threshold C for CB.  Default 3 (Ni et al.). */
  void SetCounterThreshold (uint32_t c) { m_counterThreshold = c; }
  uint32_t GetCounterThreshold () const { return m_counterThreshold; }

  /** Upper bound of the uniform random assessment delay. */
  void SetMaxRad (Time rad) { m_maxRad = rad; }
  Time GetMaxRad () const { return m_maxRad; }

  /** Communication range used by the SBA coverage proxy. */
  void SetCommRange (double range) { m_commRange = range; }

  /**
   * Bind the policy RNG to a deterministic stream so a given run seed always
   * reproduces the same PR draws and RAD samples.
   */
  void AssignStreams (int64_t stream);

  /** Draw one random assessment delay in [0, m_maxRad]. */
  Time DrawRad ();

  /** Count every reception of a packet, including duplicates. */
  void RecordReception (uint32_t originId, uint32_t sequenceNumber);

  /** Number of copies of a packet observed so far (>= 1 after first sight). */
  uint32_t GetDuplicateCount (uint32_t originId, uint32_t sequenceNumber) const;

  /** Forget per-packet state once a decision has been taken. */
  void ForgetPacket (uint32_t originId, uint32_t sequenceNumber);

  /**
   * Immediate policies (SF, PR): decide without deferring.
   * Returns true when the packet must be rebroadcast.
   */
  bool DecideImmediate ();

  /**
   * Deferred policies (CB, SBA): decide once the RAD has elapsed.
   *
   * \param originId        packet origin
   * \param sequenceNumber  packet sequence number
   * \param additionalCover number of own neighbours the previous relay could
   *                        not have reached (SBA only; ignored by CB)
   */
  bool DecideAfterRad (uint32_t originId, uint32_t sequenceNumber,
                       uint32_t additionalCover);

  /**
   * SBA coverage proxy: how many of my neighbours lie outside the previous
   * relay's transmission sphere.
   *
   * The original SBA compares explicit neighbour lists carried in hello
   * messages.  The artifact's beacons do not carry neighbour lists, so this
   * geometric test is used instead: a neighbour is "additionally covered" when
   * its distance to the previous relay exceeds the communication range.  This
   * is an approximation and is reported as such.
   */
  template <typename NeighborMap>
  uint32_t AdditionalCoverage (const NeighborMap &neighbors,
                               const Vector3D &previousRelayPos) const
  {
    uint32_t uncovered = 0;
    for (const auto &pair : neighbors)
      {
        const Vector3D &p = pair.second.position;
        double dx = p.x - previousRelayPos.x;
        double dy = p.y - previousRelayPos.y;
        double dz = p.z - previousRelayPos.z;
        if (std::sqrt (dx * dx + dy * dy + dz * dz) > m_commRange)
          {
            uncovered++;
          }
      }
    return uncovered;
  }

private:
  BroadcastBaseline m_mode;
  double m_rebroadcastProbability;
  uint32_t m_counterThreshold;
  Time m_maxRad;
  double m_commRange;

  Ptr<UniformRandomVariable> m_uniform;

  // Copies of each packet observed, keyed by (originId, sequenceNumber).
  std::map<std::pair<uint32_t, uint32_t>, uint32_t> m_duplicateCounts;
};

} // namespace astro
} // namespace ns3

#endif /* BROADCAST_BASELINES_H */
