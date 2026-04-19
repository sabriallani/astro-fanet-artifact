/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET/A3D-BSM: ns-3 integration protocol
 * Integrates beacon exchange, A3D-BSM, trust scoring, and legacy routing helpers.
 */
#ifndef ASTRO_ROUTING_PROTOCOL_H
#define ASTRO_ROUTING_PROTOCOL_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-header.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/timer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/traced-callback.h"
#include "ns3/energy-source.h"
#include "ns3/callback.h"

#include "astro-packet.h"
#include "slm-emulator.h"
#include "mappo-agent.h"
#include "a3d-bsm.h"
#include "trust-manager.h"

#include <map>
#include <set>
#include <queue>
#include <vector>

namespace ns3 {
namespace astro {

// Per-packet queue entry with traffic class priority
struct QueueEntry
{
  Ptr<Packet> packet;
  Ipv4Header ipHeader;
  TrafficClass trafficClass;
  Time enqueueTime;
  Ipv4RoutingProtocol::UnicastForwardCallback ucb;
  Ipv4RoutingProtocol::ErrorCallback ecb;

  bool operator< (const QueueEntry &other) const
  {
    // Higher priority class = lower numeric value = should dequeue first
    return PRIORITY_LEVELS[trafficClass] < PRIORITY_LEVELS[other.trafficClass];
  }
};

// Neighbor table entry
struct NeighborEntry
{
  uint32_t nodeId;
  Ipv4Address address;
  Vector3D position;
  Vector3D velocity;
  double energy;
  double linkQuality;
  MissionRole role;
  Time lastBeacon;
  AstroBeaconHeader lastBeaconHeader;
  std::vector<float> compressedEmbedding;
};

/**
 * \brief ns-3 integration protocol for the A3D-BSM simulation artifact
 *
 * This class wires the A3D-BSM suppression layer into an Ipv4RoutingProtocol
 * so it can be exercised in end-to-end FANET scenarios.
 *
 * Key features:
 * - Periodic beaconing with intent/security fields
 * - A3D-BSM broadcast suppression
 * - HMAC-style authentication and trust scoring
 * - Priority-aware packet queuing (4 traffic classes)
 * - Metrics collection (PDR, delay, throughput, BRR, energy, overhead)
 *
 * Older MAPPO/SLM helper classes remain in the executable scaffold for
 * route-selection compatibility, but they are not used to calibrate A3D-BSM.
 */
class AstroRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId (void);
  static const uint16_t ASTRO_PORT;

  AstroRoutingProtocol ();
  virtual ~AstroRoutingProtocol ();

  // Ipv4RoutingProtocol interface
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header,
                                       Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header,
                            Ptr<const NetDevice> idev,
                            UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                            LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream,
                                   Time::Unit unit = Time::S) const;

  // ASTRO-specific configuration
  void SetSinkAddress (Ipv4Address addr) { m_sinkAddress = addr; }
  Ipv4Address GetSinkAddress () const { return m_sinkAddress; }

  void SetSlmEmulator (Ptr<SlmEmulator> slm) { m_slmEmulator = slm; }
  void SetEnergySource (Ptr<EnergySource> source) { m_energySource = source; }

  // Byzantine simulation
  void SetByzantine (bool isByz, double dropRate = 0.5);

  // Statistics getters
  uint32_t GetTotalPacketsSent () const { return m_totalPacketsSent; }
  uint32_t GetTotalPacketsReceived () const { return m_totalPacketsReceived; }
  uint32_t GetTotalPacketsDropped () const { return m_totalPacketsDropped; }
  uint32_t GetTotalBroadcasts () const { return m_totalBroadcasts; }
  uint32_t GetSuppressedBroadcasts () const { return m_suppressedBroadcasts; }
  uint64_t GetTotalControlBytes () const { return m_totalControlBytes; }
  uint64_t GetTotalDataBytes () const { return m_totalDataBytes; }
  double GetAverageDelay () const;
  double GetBroadcastRedundancyRatio () const;

  // Metrics output
  void PrintMetrics (Ptr<OutputStreamWrapper> stream) const;

protected:
  virtual void DoInitialize (void);
  virtual void DoDispose (void);

private:
  // ---- Layer components ----
  Ptr<SlmEmulator> m_slmEmulator;
  Ptr<MappoAgent> m_mappoAgent;
  Ptr<A3dBsm> m_a3dBsm;
  Ptr<TrustManager> m_trustManager;

  // ---- Network state ----
  Ptr<Ipv4> m_ipv4;
  Ptr<Socket> m_socket;          // UDP socket for control messages
  Ptr<EnergySource> m_energySource;

  uint32_t m_nodeId;
  Ipv4Address m_mainAddress;
  Ipv4Address m_sinkAddress;
  MissionRole m_currentRole;

  // Neighbor table
  std::map<uint32_t, NeighborEntry> m_neighborTable;
  Time m_neighborTimeout;

  // Priority packet queue (per traffic class)
  std::priority_queue<QueueEntry> m_packetQueue;
  uint32_t m_maxQueueSize;
  uint32_t m_queueSizes[4];  // Per-class queue lengths

  // Legacy scaffold context vector
  std::vector<float> m_currentEmbedding;

  // ---- Timers ----
  Timer m_beaconTimer;
  Timer m_decisionTimer;
  Time m_beaconInterval;      // Beacon period
  Time m_decisionEpoch;       // Decision epoch (200ms from Table 2)

  // ---- Sequence numbers ----
  uint32_t m_seqNo;

  // ---- Statistics ----
  uint32_t m_totalPacketsSent;
  uint32_t m_totalPacketsReceived;
  uint32_t m_totalPacketsDropped;
  uint32_t m_totalBroadcasts;
  uint32_t m_suppressedBroadcasts;
  uint64_t m_totalControlBytes;
  uint64_t m_totalDataBytes;
  std::vector<double> m_deliveryDelays;

  // ---- Duplicate detection ----
  std::set<std::pair<uint32_t, uint32_t>> m_seenPackets;  // (originId, seqNo)

  // ---- Core methods ----

  /**
   * Send periodic beacon with intent vector and compressed embedding.
   */
  void SendBeacon ();

  /**
   * Handle received beacon from neighbor.
   */
  void HandleBeacon (Ptr<Socket> socket);

  /**
   * Execute one decision epoch.
   * Called every m_decisionEpoch (200ms).
   */
  void ExecuteDecisionCycle ();

  /**
   * Build the raw state vector s_i(t) from current node state.
   */
  RawStateVector BuildRawState () const;

  /**
   * Aggregate trusted neighbor intents (mean pooling).
   */
  std::vector<float> AggregateNeighborIntents () const;

  /**
   * Build the neighbor info vector for the legacy route-selection helper.
   */
  std::vector<NeighborInfo> BuildNeighborInfoVector () const;

  /**
   * Execute the selected action.
   */
  void ExecuteAction (const PolicyAction &action);

  /**
   * Forward a packet to a specific neighbor.
   */
  void ForwardPacket (Ptr<Packet> packet, const Ipv4Header &header,
                      Ipv4Address nextHop);

  /**
   * Broadcast a packet (with A3D-BSM evaluation).
   */
  void BroadcastPacket (Ptr<Packet> packet, const Ipv4Header &header);

  /**
   * Handle conflict resolution for forwarding (Eq. 15).
   */
  Ipv4Address ResolveConflict (const std::vector<uint32_t> &contenders,
                                Ipv4Address intendedNextHop) const;

  /**
   * Clean up expired neighbor entries.
   */
  void PurgeNeighborTable ();

  /**
   * Estimate local UAV density.
   */
  double EstimateLocalDensity () const;

  /**
   * Compute mobility gradient from velocity history.
   */
  Vector3D ComputeMobilityGradient () const;

  /**
   * Get current position from mobility model.
   */
  Vector3D GetCurrentPosition () const;

  /**
   * Get current velocity from mobility model.
   */
  Vector3D GetCurrentVelocity () const;

  /**
   * Get normalized residual energy.
   */
  double GetResidualEnergy () const;

  /**
   * Estimate link quality to a neighbor (based on distance and SNR model).
   */
  double EstimateLinkQuality (const Vector3D &neighborPos) const;
};

} // namespace astro
} // namespace ns3

#endif /* ASTRO_ROUTING_PROTOCOL_H */
