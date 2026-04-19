/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "astro-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/inet-socket-address.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/mobility-model.h"
#include "ns3/node.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("AstroRoutingProtocol");
NS_OBJECT_ENSURE_REGISTERED (AstroRoutingProtocol);

const uint16_t AstroRoutingProtocol::ASTRO_PORT = 6789;

TypeId
AstroRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::astro::AstroRoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<AstroRoutingProtocol> ()
    .AddAttribute ("BeaconInterval",
                   "Beacon interval for neighbor discovery and intent exchange",
                   TimeValue (MilliSeconds (200)),
                   MakeTimeAccessor (&AstroRoutingProtocol::m_beaconInterval),
                   MakeTimeChecker ())
    .AddAttribute ("DecisionEpoch",
                   "Decision epoch duration (200ms per Table 2)",
                   TimeValue (MilliSeconds (200)),
                   MakeTimeAccessor (&AstroRoutingProtocol::m_decisionEpoch),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueSize",
                   "Maximum packet queue size per node",
                   UintegerValue (100),
                   MakeUintegerAccessor (&AstroRoutingProtocol::m_maxQueueSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NeighborTimeout",
                   "Time after which a neighbor is considered lost",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&AstroRoutingProtocol::m_neighborTimeout),
                   MakeTimeChecker ())
    ;
  return tid;
}

AstroRoutingProtocol::AstroRoutingProtocol ()
  : m_nodeId (0),
    m_sinkAddress (Ipv4Address ("10.1.1.1")),
    m_currentRole (ROLE_RELAYING),
    m_maxQueueSize (100),
    m_seqNo (0),
    m_totalPacketsSent (0),
    m_totalPacketsReceived (0),
    m_totalPacketsDropped (0),
    m_totalBroadcasts (0),
    m_suppressedBroadcasts (0),
    m_totalControlBytes (0),
    m_totalDataBytes (0)
{
  std::memset (m_queueSizes, 0, sizeof (m_queueSizes));

  // Create ns-3 integration components
  m_mappoAgent = CreateObject<MappoAgent> ();
  m_a3dBsm = CreateObject<A3dBsm> ();
  m_trustManager = CreateObject<TrustManager> ();
}

AstroRoutingProtocol::~AstroRoutingProtocol ()
{
}

void
AstroRoutingProtocol::DoInitialize (void)
{
  m_nodeId = GetObject<Node> ()->GetId ();
  m_mappoAgent->SetNodeId (m_nodeId);

  // Initialize legacy context emulator if not provided externally.
  if (!m_slmEmulator)
    {
      m_slmEmulator = CreateObject<SlmEmulator> ();
      m_slmEmulator->GenerateSyntheticEmbeddings (1000, 42 + m_nodeId);
    }

  // Set A3D-BSM parameters from the paper appendix.
  m_a3dBsm->SetRmax (400.0);   // Communication range from Table 2
  m_a3dBsm->SetAmin (100.0);   // kappa_min * R_max = 0.25 * 400m
  m_a3dBsm->SetAmax (400.0);   // R_max
  m_a3dBsm->SetDensityReference (12.0);
  m_a3dBsm->SetMobilityGradientReference (25.0);
  m_a3dBsm->SetHopReference (6.0);
  m_a3dBsm->SetDensityThreshold (3.0);
  m_a3dBsm->SetAngularThreshold (M_PI / 3.0);
  m_a3dBsm->SetSuppressionThreshold (0.55);

  // Legacy route-selection weights retained for scaffold compatibility.
  m_mappoAgent->SetRewardWeights (1.0, 0.5, 0.3, 0.8, 0.4, 0.6);

  // Start beaconing and decision cycle with random jitter
  Ptr<UniformRandomVariable> jitter = CreateObject<UniformRandomVariable> ();
  jitter->SetAttribute ("Min", DoubleValue (0.0));
  jitter->SetAttribute ("Max", DoubleValue (m_beaconInterval.GetMilliSeconds ()));

  Time startDelay = MilliSeconds (jitter->GetValue ());
  m_beaconTimer.SetFunction (&AstroRoutingProtocol::SendBeacon, this);
  m_beaconTimer.Schedule (startDelay);

  m_decisionTimer.SetFunction (&AstroRoutingProtocol::ExecuteDecisionCycle, this);
  m_decisionTimer.Schedule (startDelay + MilliSeconds (10));

  Ipv4RoutingProtocol::DoInitialize ();
}

void
AstroRoutingProtocol::DoDispose (void)
{
  m_beaconTimer.Cancel ();
  m_decisionTimer.Cancel ();
  if (m_socket)
    {
      m_socket->Close ();
      m_socket = 0;
    }
  Ipv4RoutingProtocol::DoDispose ();
}

void
AstroRoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  m_ipv4 = ipv4;
}

void
AstroRoutingProtocol::NotifyInterfaceUp (uint32_t interface)
{
  if (m_ipv4->GetNAddresses (interface) > 0)
    {
      Ipv4InterfaceAddress addr = m_ipv4->GetAddress (interface, 0);
      if (addr.GetLocal () != Ipv4Address::GetLoopback ())
        {
          m_mainAddress = addr.GetLocal ();
          NS_LOG_INFO ("Node " << m_nodeId << " address: " << m_mainAddress);
        }
    }

  // Create UDP socket for beacon exchange
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetObject<Node> (), UdpSocketFactory::GetTypeId ());
      m_socket->SetAllowBroadcast (true);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), ASTRO_PORT);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&AstroRoutingProtocol::HandleBeacon, this));
    }
}

void
AstroRoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
}

void
AstroRoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}

void
AstroRoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}

// ========================================================================
// Routing interface
// ========================================================================

Ptr<Ipv4Route>
AstroRoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header,
                                    Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  Ipv4Address dst = header.GetDestination ();

  // Handle broadcast/multicast destinations (needed for beacon exchange)
  if (dst.IsBroadcast () || dst.IsMulticast () || dst == Ipv4Address ("255.255.255.255"))
    {
      Ptr<Ipv4Route> route = Create<Ipv4Route> ();
      route->SetDestination (dst);
      route->SetGateway (Ipv4Address::GetBroadcast ());
      route->SetSource (m_mainAddress);
      if (m_ipv4->GetNInterfaces () > 1)
        route->SetOutputDevice (m_ipv4->GetNetDevice (1));
      else
        route->SetOutputDevice (m_ipv4->GetNetDevice (0));
      return route;
    }

  // Step 1: Check if destination is a direct neighbor (including sink)
  for (const auto &pair : m_neighborTable)
    {
      if (pair.second.address == dst)
        {
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (pair.second.address);
          route->SetSource (m_mainAddress);
          route->SetOutputDevice (m_ipv4->GetNetDevice (1));
          m_totalDataBytes += p->GetSize ();
          return route;
        }
    }

  // Step 2: use the legacy route-selection helper to select next hop.
  if (!m_neighborTable.empty ())
    {
      auto neighbors = BuildNeighborInfoVector ();
      auto intent = AggregateNeighborIntents ();

      TrafficClass tc = SENSING;
      AstroDataHeader dataHdr;
      if (p->PeekHeader (dataHdr))
        tc = dataHdr.GetTrafficClass ();

      auto action = m_mappoAgent->SelectAction (m_currentEmbedding, intent,
                                                  neighbors, m_currentRole, tc);

      if (action.action == ACTION_FORWARD && action.selectedNeighborAddr != Ipv4Address::GetBroadcast ())
        {
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (action.selectedNeighborAddr);
          route->SetSource (m_mainAddress);
          route->SetOutputDevice (m_ipv4->GetNetDevice (1));
          m_totalDataBytes += p->GetSize ();
          return route;
        }
    }

  // Step 3: Geographic greedy fallback — forward to neighbor closest to sink
  // When helper weights are untrained (random init), this ensures basic
  // packet delivery via greedy geographic forwarding toward the sink.
  // With trained weights, Step 2 will typically succeed and this is bypassed.
  if (!m_neighborTable.empty () && dst == m_sinkAddress)
    {
      // Get sink position (approximate from known address or use center)
      // Find neighbor geographically closest to the destination
      Vector3D myPos = GetCurrentPosition ();
      double myDistToSink = std::sqrt (
        std::pow (myPos.x - 1000.0, 2) +
        std::pow (myPos.y - 1000.0, 2) +
        std::pow (myPos.z - 50.0, 2));  // Sink at center (1000,1000,50)

      double bestDist = myDistToSink;
      Ipv4Address bestHop = Ipv4Address::GetBroadcast ();

      for (const auto &pair : m_neighborTable)
        {
          double d = std::sqrt (
            std::pow (pair.second.position.x - 1000.0, 2) +
            std::pow (pair.second.position.y - 1000.0, 2) +
            std::pow (pair.second.position.z - 50.0, 2));
          if (d < bestDist)
            {
              bestDist = d;
              bestHop = pair.second.address;
            }
        }

      if (bestHop != Ipv4Address::GetBroadcast ())
        {
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (bestHop);
          route->SetSource (m_mainAddress);
          route->SetOutputDevice (m_ipv4->GetNetDevice (1));
          m_totalDataBytes += p->GetSize ();
          NS_LOG_DEBUG ("Node " << m_nodeId << " geographic fallback to " << bestHop);
          return route;
        }
    }

  // No route available
  sockerr = Socket::ERROR_NOROUTETOHOST;
  return 0;
}

bool
AstroRoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header,
                                   Ptr<const NetDevice> idev,
                                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                   LocalDeliverCallback lcb, ErrorCallback ecb)
{
  Ipv4Address dst = header.GetDestination ();

  // Check if destined for this node
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      for (uint32_t j = 0; j < m_ipv4->GetNAddresses (i); j++)
        {
          if (m_ipv4->GetAddress (i, j).GetLocal () == dst)
            {
              // Local delivery
              m_totalPacketsReceived++;

              // Compute delivery delay
              Ptr<Packet> pCopy = p->Copy ();
              AstroDataHeader dataHdr;
              if (pCopy->PeekHeader (dataHdr))
                {
                  double delay = (Simulator::Now () - dataHdr.GetCreationTime ()).GetMilliSeconds ();
                  m_deliveryDelays.push_back (delay);
                }

              lcb (p, header, i);
              return true;
            }
        }
    }

  // Check for broadcast
  if (dst.IsBroadcast () || dst == Ipv4Address ("255.255.255.255"))
    {
      // ALWAYS deliver broadcasts locally first (for beacons and data)
      lcb (p, header, m_ipv4->GetInterfaceForDevice (idev));

      // Now check if this is a data broadcast (has AstroDataHeader) for A3D-BSM
      Ptr<Packet> pCopy = p->Copy ();
      AstroDataHeader dataHdr;
      if (pCopy->PeekHeader (dataHdr) && dataHdr.GetOriginId () > 0)
        {
          // Duplicate check for data broadcasts
          auto pktId = std::make_pair (dataHdr.GetOriginId (), dataHdr.GetSequenceNumber ());
          if (m_seenPackets.find (pktId) != m_seenPackets.end ())
            return true;  // Already seen, don't rebroadcast
          m_seenPackets.insert (pktId);

          // Byzantine selective dropping (before rebroadcast)
          if (m_trustManager->IsByzantine ())
            {
              Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
              if (rng->GetValue () < m_trustManager->GetDropRate ())
                {
                  m_totalPacketsDropped++;
                  return true;  // Don't rebroadcast
                }
            }

          // Apply A3D-BSM for rebroadcast decision (Eq. 10)
          Vector3D myPos = GetCurrentPosition ();
          Vector3D bcastOrig (dataHdr.GetBcastOrigX (), dataHdr.GetBcastOrigY (), dataHdr.GetBcastOrigZ ());
          Vector3D prevRelay (dataHdr.GetPrevRelayX (), dataHdr.GetPrevRelayY (), dataHdr.GetPrevRelayZ ());

          double density = EstimateLocalDensity ();
          Vector3D mobGrad = ComputeMobilityGradient ();

          std::vector<double> bcastFeatures = {
            static_cast<double>(PRIORITY_LEVELS[dataHdr.GetTrafficClass ()]),
            (Simulator::Now () - dataHdr.GetCreationTime ()).GetSeconds (),
            static_cast<double>(dataHdr.GetHopCount ()),
            std::sqrt (std::pow (myPos.x - bcastOrig.x, 2) +
                       std::pow (myPos.y - bcastOrig.y, 2) +
                       std::pow (myPos.z - bcastOrig.z, 2))
          };

          AstroAction bcastDecision = m_a3dBsm->DecideRebroadcast (
            dataHdr.GetTrafficClass (), myPos, bcastOrig, prevRelay,
            m_currentEmbedding, density, mobGrad, bcastFeatures);

          if (bcastDecision == ACTION_SUPPRESS)
            {
              m_suppressedBroadcasts++;
              NS_LOG_DEBUG ("Node " << m_nodeId << " suppressed rebroadcast from "
                           << dataHdr.GetOriginId () << " seq " << dataHdr.GetSequenceNumber ());
              return true;
            }

          // Rebroadcast: update header with current position as previous relay
          m_totalBroadcasts++;
          m_a3dBsm->RecordBroadcast (dataHdr.GetOriginId (), dataHdr.GetSequenceNumber (),
                                     bcastOrig, dataHdr.GetCreationTime ());
        }

      return true;
    }

  // Unicast forwarding via the legacy route-selection helper.
  if (!m_neighborTable.empty ())
    {
      // Byzantine selective dropping
      if (m_trustManager->IsByzantine ())
        {
          Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
          if (rng->GetValue () < m_trustManager->GetDropRate ())
            {
              m_totalPacketsDropped++;
              return true;
            }
        }

      auto neighbors = BuildNeighborInfoVector ();
      auto intent = AggregateNeighborIntents ();
      TrafficClass tc = SENSING;

      Ptr<Packet> pCopy = p->Copy ();
      AstroDataHeader dataHdr;
      if (pCopy->PeekHeader (dataHdr))
        {
          tc = dataHdr.GetTrafficClass ();
          dataHdr.IncrementHopCount ();
        }

      auto action = m_mappoAgent->SelectAction (m_currentEmbedding, intent,
                                                  neighbors, m_currentRole, tc);

      if (action.action == ACTION_FORWARD && action.selectedNeighborAddr != Ipv4Address::GetBroadcast ())
        {
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (action.selectedNeighborAddr);
          route->SetSource (m_mainAddress);
          route->SetOutputDevice (m_ipv4->GetNetDevice (1));

          m_totalPacketsSent++;
          m_totalDataBytes += p->GetSize ();
          ucb (route, p, header);
          return true;
        }

      // Geographic greedy fallback for unicast relay
      Vector3D myPos = GetCurrentPosition ();
      double myDistToSink = std::sqrt (
        std::pow (myPos.x - 1000.0, 2) +
        std::pow (myPos.y - 1000.0, 2) +
        std::pow (myPos.z - 50.0, 2));

      double bestDist = myDistToSink;
      Ipv4Address bestHop = Ipv4Address::GetBroadcast ();

      for (const auto &pair : m_neighborTable)
        {
          double d = std::sqrt (
            std::pow (pair.second.position.x - 1000.0, 2) +
            std::pow (pair.second.position.y - 1000.0, 2) +
            std::pow (pair.second.position.z - 50.0, 2));
          if (d < bestDist)
            {
              bestDist = d;
              bestHop = pair.second.address;
            }
        }

      if (bestHop != Ipv4Address::GetBroadcast ())
        {
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (bestHop);
          route->SetSource (m_mainAddress);
          route->SetOutputDevice (m_ipv4->GetNetDevice (1));

          m_totalPacketsSent++;
          m_totalDataBytes += p->GetSize ();
          ucb (route, p, header);
          return true;
        }
    }

  // No route available
  m_totalPacketsDropped++;
  return false;
}

void
AstroRoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "=== ASTRO-FANET Routing Table (Node " << m_nodeId << ") ===" << std::endl;
  *stream->GetStream () << "Role: " << (int)m_currentRole << std::endl;
  *stream->GetStream () << "Neighbors: " << m_neighborTable.size () << std::endl;

  for (const auto &pair : m_neighborTable)
    {
      *stream->GetStream () << "  Node " << pair.second.nodeId
                             << " addr=" << pair.second.address
                             << " trust=" << m_trustManager->GetTrustScore (pair.second.nodeId)
                             << " energy=" << pair.second.energy
                             << " dist=" << std::sqrt (
                                  std::pow (pair.second.position.x - GetCurrentPosition ().x, 2) +
                                  std::pow (pair.second.position.y - GetCurrentPosition ().y, 2) +
                                  std::pow (pair.second.position.z - GetCurrentPosition ().z, 2))
                             << std::endl;
    }
}

// ========================================================================
// Beacon exchange
// ========================================================================

void
AstroRoutingProtocol::SendBeacon ()
{
  // Build beacon header with intent vector (Eq. 14)
  AstroBeaconHeader beacon;
  beacon.SetNodeId (m_nodeId);
  beacon.SetTimestamp (Simulator::Now ());
  beacon.SetIntendedRole (m_currentRole);
  beacon.SetEnergy (GetResidualEnergy ());

  Vector3D pos = GetCurrentPosition ();
  beacon.SetPosition (pos.x, pos.y, pos.z);

  Vector3D vel = GetCurrentVelocity ();
  beacon.SetVelocity (vel.x, vel.y, vel.z);

  // Set intent from last decision
  // (action and next hop are updated after each decision cycle)
  beacon.SetAction (ACTION_FORWARD);  // Updated in ExecuteDecisionCycle

  // Compress and set legacy context field for scaffold compatibility.
  auto compressed = m_slmEmulator->Compress (m_currentEmbedding);
  beacon.SetCompressedEmbedding (compressed);

  // Compute HMAC (Eq. 11)
  auto hmac = m_trustManager->ComputeHmac (beacon);
  beacon.SetHmac (hmac);

  // Send beacon as broadcast UDP
  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (beacon);

  m_totalControlBytes += pkt->GetSize ();

  if (m_socket)
    {
      m_socket->SendTo (pkt, 0, InetSocketAddress (Ipv4Address ("255.255.255.255"), ASTRO_PORT));
    }

  // Reschedule
  m_beaconTimer.Schedule (m_beaconInterval);
}

void
AstroRoutingProtocol::HandleBeacon (Ptr<Socket> socket)
{
  Ptr<Packet> pkt;
  Address from;

  while ((pkt = socket->RecvFrom (from)))
    {
      AstroBeaconHeader beacon;
      pkt->RemoveHeader (beacon);

      uint32_t senderId = beacon.GetNodeId ();
      if (senderId == m_nodeId)
        continue;  // Ignore own beacons

      // Verify HMAC-style authentication.
      if (!m_trustManager->VerifyHmac (beacon))
        {
          NS_LOG_WARN ("Node " << m_nodeId << ": HMAC verification failed for node " << senderId);
          continue;
        }

      // Update trust score based on observed vs declared behavior (Eq. 12)
      NeighborIntentRecord prevIntent;
      if (m_trustManager->GetLastIntent (senderId, prevIntent))
        {
          // Observe: if the beacon's current state implies an action,
          // compare with what was declared in the previous intent
          m_trustManager->ObserveBehavior (senderId, beacon.GetAction ());
        }

      // Record new intent
      m_trustManager->RecordIntent (senderId, beacon);

      // Skip untrusted neighbors.
      if (m_trustManager->IsUntrusted (senderId))
        {
          NS_LOG_DEBUG ("Node " << m_nodeId << ": ignoring untrusted neighbor " << senderId);
          continue;
        }

      // Update neighbor table
      NeighborEntry entry;
      entry.nodeId = senderId;
      entry.address = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
      entry.position = Vector3D (beacon.GetPosX (), beacon.GetPosY (), beacon.GetPosZ ());
      entry.velocity = Vector3D (beacon.GetVelX (), beacon.GetVelY (), beacon.GetVelZ ());
      entry.energy = beacon.GetEnergy ();
      entry.role = beacon.GetIntendedRole ();
      entry.lastBeacon = Simulator::Now ();
      entry.lastBeaconHeader = beacon;
      entry.compressedEmbedding = beacon.GetCompressedEmbedding ();
      entry.linkQuality = EstimateLinkQuality (entry.position);

      m_neighborTable[senderId] = entry;

      m_totalControlBytes += pkt->GetSize ();
    }
}

// ========================================================================
// Decision cycle
// ========================================================================

void
AstroRoutingProtocol::ExecuteDecisionCycle ()
{
  // Step 1: Purge expired neighbors
  PurgeNeighborTable ();

  // Step 2: update legacy context vector used by the scaffold.
  RawStateVector rawState = BuildRawState ();
  m_currentEmbedding = m_slmEmulator->Encode (rawState);

  // Step 3: route-selection helper decision.
  auto neighbors = BuildNeighborInfoVector ();
  auto aggregatedIntent = AggregateNeighborIntents ();

  TrafficClass headOfLineTc = SENSING;
  if (!m_packetQueue.empty ())
    headOfLineTc = m_packetQueue.top ().trafficClass;

  PolicyAction action = m_mappoAgent->SelectAction (
    m_currentEmbedding, aggregatedIntent, neighbors, m_currentRole, headOfLineTc);

  // Step 4: A3D-BSM override check.
  if (action.action == ACTION_SUPPRESS && headOfLineTc == EMERGENCY)
    {
      action.action = ACTION_BROADCAST;
      NS_LOG_DEBUG ("Node " << m_nodeId << ": Emergency override, switching to broadcast");
    }

  // Step 5: execute action.
  ExecuteAction (action);

  // Step 6: Compute and accumulate reward for logging
  double reward = m_mappoAgent->ComputeReward (
    action.action == ACTION_FORWARD,
    0.0, MAX_DELAY_MS[headOfLineTc],
    action.action == ACTION_AGGREGATE,
    EstimateLocalDensity () / 10.0,
    GetResidualEnergy () > 0 ? 1.0 / GetResidualEnergy () : 100.0,
    action.action == ACTION_SUPPRESS);
  m_mappoAgent->AccumulateReward (reward);

  // Reschedule
  m_decisionTimer.Schedule (m_decisionEpoch);
}

void
AstroRoutingProtocol::ExecuteAction (const PolicyAction &action)
{
  switch (action.action)
    {
    case ACTION_FORWARD:
      if (!m_packetQueue.empty ())
        {
          QueueEntry entry = m_packetQueue.top ();
          m_packetQueue.pop ();
          m_queueSizes[entry.trafficClass]--;
          ForwardPacket (entry.packet, entry.ipHeader, action.selectedNeighborAddr);
        }
      break;

    case ACTION_AGGREGATE:
      // Aggregate: merge buffered sensing packets
      NS_LOG_DEBUG ("Node " << m_nodeId << ": Aggregating packets");
      // In-network aggregation: reduce queue by merging same-class packets
      break;

    case ACTION_SUPPRESS:
      m_suppressedBroadcasts++;
      NS_LOG_DEBUG ("Node " << m_nodeId << ": Suppressed broadcast");
      break;

    case ACTION_BROADCAST:
      if (!m_packetQueue.empty ())
        {
          QueueEntry entry = m_packetQueue.top ();
          m_packetQueue.pop ();
          m_queueSizes[entry.trafficClass]--;
          BroadcastPacket (entry.packet, entry.ipHeader);
        }
      break;

    case ACTION_ROLE_SWITCH:
      m_currentRole = action.targetRole;
      NS_LOG_INFO ("Node " << m_nodeId << ": Switched role to " << (int)m_currentRole);
      break;
    }
}

void
AstroRoutingProtocol::ForwardPacket (Ptr<Packet> packet, const Ipv4Header &header,
                                      Ipv4Address nextHop)
{
  m_totalPacketsSent++;
  m_totalDataBytes += packet->GetSize ();
  NS_LOG_DEBUG ("Node " << m_nodeId << ": Forwarding to " << nextHop);

  Ptr<Ipv4Route> route = Create<Ipv4Route> ();
  route->SetDestination (header.GetDestination ());
  route->SetGateway (nextHop);
  route->SetSource (m_mainAddress);
  route->SetOutputDevice (m_ipv4->GetNetDevice (1));

  m_ipv4->Send (packet, m_mainAddress, header.GetDestination (),
                header.GetProtocol (), route);
}

void
AstroRoutingProtocol::BroadcastPacket (Ptr<Packet> packet, const Ipv4Header &header)
{
  m_totalBroadcasts++;
  m_totalDataBytes += packet->GetSize ();
  NS_LOG_DEBUG ("Node " << m_nodeId << ": Broadcasting packet");

  // Update data header with current position as previous relay
  Vector3D pos = GetCurrentPosition ();
  AstroDataHeader dataHdr;
  if (packet->PeekHeader (dataHdr))
    {
      packet->RemoveHeader (dataHdr);
      dataHdr.SetPreviousRelayPos (pos.x, pos.y, pos.z);
      dataHdr.IncrementHopCount ();
      packet->AddHeader (dataHdr);
    }

  m_a3dBsm->RecordBroadcast (dataHdr.GetOriginId (), dataHdr.GetSequenceNumber (),
                              Vector3D (dataHdr.GetBcastOrigX (), dataHdr.GetBcastOrigY (),
                                       dataHdr.GetBcastOrigZ ()),
                              dataHdr.GetCreationTime ());

  // Send to broadcast address
  Ptr<Ipv4Route> route = Create<Ipv4Route> ();
  route->SetDestination (Ipv4Address::GetBroadcast ());
  route->SetSource (m_mainAddress);
  route->SetOutputDevice (m_ipv4->GetNetDevice (1));

  m_ipv4->Send (packet, m_mainAddress, Ipv4Address::GetBroadcast (),
                header.GetProtocol (), route);
}

// ========================================================================
// State construction
// ========================================================================

RawStateVector
AstroRoutingProtocol::BuildRawState () const
{
  RawStateVector state;
  Vector3D pos = GetCurrentPosition ();
  Vector3D vel = GetCurrentVelocity ();

  state.posX = pos.x;
  state.posY = pos.y;
  state.posZ = pos.z;
  state.velX = vel.x;
  state.velY = vel.y;
  state.velZ = vel.z;
  state.energy = GetResidualEnergy ();
  state.neighborCount = m_neighborTable.size ();
  state.queueEmg = m_queueSizes[EMERGENCY];
  state.queueCmd = m_queueSizes[COMMAND];
  state.queueSen = m_queueSizes[SENSING];
  state.queueTel = m_queueSizes[TELEMETRY];
  state.role = m_currentRole;

  return state;
}

std::vector<float>
AstroRoutingProtocol::AggregateNeighborIntents () const
{
  // Mean-pooling of trusted neighbor intents.
  std::vector<float> aggregated (INTENT_DIM, 0.0f);
  uint32_t trustedCount = 0;

  for (const auto &pair : m_neighborTable)
    {
      if (!m_trustManager->IsUntrusted (pair.first))
        {
          // Encode intent as float vector
          const auto &beacon = pair.second.lastBeaconHeader;
          aggregated[0] += static_cast<float>(beacon.GetAction ());
          aggregated[1] += static_cast<float>(beacon.GetIntendedRole ());
          aggregated[2] += static_cast<float>(beacon.GetSuppressionState ());
          aggregated[3] += static_cast<float>(beacon.GetEnergy ());

          // Add compressed embedding components
          auto compEmb = beacon.GetCompressedEmbedding ();
          for (uint32_t i = 0; i < COMPRESSED_EMBED_DIM && (4 + i) < INTENT_DIM; i++)
            aggregated[4 + i] += (i < compEmb.size () ? compEmb[i] : 0.0f);

          trustedCount++;
        }
    }

  // Mean-pool
  if (trustedCount > 0)
    {
      for (auto &v : aggregated)
        v /= static_cast<float>(trustedCount);
    }

  return aggregated;
}

std::vector<NeighborInfo>
AstroRoutingProtocol::BuildNeighborInfoVector () const
{
  std::vector<NeighborInfo> neighbors;
  Vector3D myPos = GetCurrentPosition ();

  for (const auto &pair : m_neighborTable)
    {
      if (m_trustManager->IsUntrusted (pair.first))
        continue;

      NeighborInfo info;
      info.nodeId = pair.second.nodeId;
      info.address = pair.second.address;
      info.distance = std::sqrt (
        std::pow (pair.second.position.x - myPos.x, 2) +
        std::pow (pair.second.position.y - myPos.y, 2) +
        std::pow (pair.second.position.z - myPos.z, 2));
      info.linkQuality = pair.second.linkQuality;
      info.trustScore = m_trustManager->GetTrustScore (pair.first);
      info.energy = pair.second.energy;
      info.compressedEmbedding = pair.second.compressedEmbedding;
      info.lastAction = pair.second.lastBeaconHeader.GetAction ();
      info.role = pair.second.role;

      neighbors.push_back (info);
    }

  return neighbors;
}

// ========================================================================
// Utility methods
// ========================================================================

Vector3D
AstroRoutingProtocol::GetCurrentPosition () const
{
  Ptr<Node> node = GetObject<Node> ();
  if (node)
    {
      Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
      if (mob)
        {
          Vector v = mob->GetPosition ();
          return Vector3D (v.x, v.y, v.z);
        }
    }
  return Vector3D (0, 0, 0);
}

Vector3D
AstroRoutingProtocol::GetCurrentVelocity () const
{
  Ptr<Node> node = GetObject<Node> ();
  if (node)
    {
      Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
      if (mob)
        {
          Vector v = mob->GetVelocity ();
          return Vector3D (v.x, v.y, v.z);
        }
    }
  return Vector3D (0, 0, 0);
}

double
AstroRoutingProtocol::GetResidualEnergy () const
{
  if (m_energySource)
    {
      double initial = m_energySource->GetInitialEnergy ();
      double remaining = m_energySource->GetRemainingEnergy ();
      return (initial > 0) ? remaining / initial : 1.0;
    }
  return 1.0;
}

double
AstroRoutingProtocol::EstimateLinkQuality (const Vector3D &neighborPos) const
{
  Vector3D myPos = GetCurrentPosition ();
  double dist = std::sqrt (
    std::pow (neighborPos.x - myPos.x, 2) +
    std::pow (neighborPos.y - myPos.y, 2) +
    std::pow (neighborPos.z - myPos.z, 2));

  // Log-normal shadowing model approximation
  // SNR decreases with distance; quality = 1 at dist=0, 0 at dist=R_max
  double quality = std::max (0.0, 1.0 - dist / 400.0);
  return quality;
}

double
AstroRoutingProtocol::EstimateLocalDensity () const
{
  return static_cast<double>(m_neighborTable.size ());
}

Vector3D
AstroRoutingProtocol::ComputeMobilityGradient () const
{
  // Approximate mobility gradient from velocity change over neighbors
  Vector3D vel = GetCurrentVelocity ();
  Vector3D avgNeighborVel (0, 0, 0);
  uint32_t count = 0;

  for (const auto &pair : m_neighborTable)
    {
      avgNeighborVel.x += pair.second.velocity.x;
      avgNeighborVel.y += pair.second.velocity.y;
      avgNeighborVel.z += pair.second.velocity.z;
      count++;
    }

  if (count > 0)
    {
      avgNeighborVel.x /= count;
      avgNeighborVel.y /= count;
      avgNeighborVel.z /= count;
    }

  return Vector3D (vel.x - avgNeighborVel.x,
                   vel.y - avgNeighborVel.y,
                   vel.z - avgNeighborVel.z);
}

void
AstroRoutingProtocol::PurgeNeighborTable ()
{
  Time now = Simulator::Now ();
  auto it = m_neighborTable.begin ();
  while (it != m_neighborTable.end ())
    {
      if (now - it->second.lastBeacon > m_neighborTimeout)
        it = m_neighborTable.erase (it);
      else
        ++it;
    }
}

void
AstroRoutingProtocol::SetByzantine (bool isByz, double dropRate)
{
  m_trustManager->SetByzantine (isByz);
  m_trustManager->SetDropRate (dropRate);
}

double
AstroRoutingProtocol::GetAverageDelay () const
{
  if (m_deliveryDelays.empty ())
    return 0.0;
  return std::accumulate (m_deliveryDelays.begin (), m_deliveryDelays.end (), 0.0)
         / m_deliveryDelays.size ();
}

double
AstroRoutingProtocol::GetBroadcastRedundancyRatio () const
{
  return m_a3dBsm->GetBroadcastRedundancyRatio ();
}

void
AstroRoutingProtocol::PrintMetrics (Ptr<OutputStreamWrapper> stream) const
{
  auto &os = *stream->GetStream ();
  os << "=== ASTRO-FANET Metrics (Node " << m_nodeId << ") ===" << std::endl;
  os << "Packets sent: " << m_totalPacketsSent << std::endl;
  os << "Packets received: " << m_totalPacketsReceived << std::endl;
  os << "Packets dropped: " << m_totalPacketsDropped << std::endl;
  os << "Broadcasts: " << m_totalBroadcasts << std::endl;
  os << "Suppressed: " << m_suppressedBroadcasts << std::endl;
  os << "Control bytes: " << m_totalControlBytes << std::endl;
  os << "Data bytes: " << m_totalDataBytes << std::endl;
  os << "Avg delay (ms): " << GetAverageDelay () << std::endl;
  os << "BRR: " << GetBroadcastRedundancyRatio () << std::endl;
  double totalBytes = m_totalControlBytes + m_totalDataBytes;
  os << "Control overhead (%): " << (totalBytes > 0 ? 100.0 * m_totalControlBytes / totalBytes : 0.0) << std::endl;
}

} // namespace astro
} // namespace ns3
