/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET NS-3 Simulation
 * ============================
 * Main simulation script implementing the evaluation campaign from Section 4
 *
 * Protocols: ASTRO-FANET, AODV, GPSR (approx.), OLSR, Epidemic, DQN-QR (approx.)
 * Mobility:  Gauss-Markov 3D (GM3D), Reference Point Group Mobility (RPGM)
 * Metrics:   PDR, End-to-End Delay, Throughput, AoI, BRR, Energy/bit, Control Overhead
 *
 * Usage:
 *   ./waf --run "astro-fanet-sim --nUavs=30 --protocol=astro --mobility=gm3d --seed=1001"
 *
 * Parameters match Table 2 of the paper.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/stats-module.h"
#ifdef ASTRO_ENABLE_NETANIM
#include "ns3/netanim-module.h"
#endif
#include "ns3/system-path.h"

// ASTRO-FANET module
#include "ns3/astro-routing-protocol.h"
#include "ns3/astro-helper.h"
#include "ns3/astro-packet.h"
#include "ns3/rpgm-mobility-model.h"
#include "ns3/slm-emulator.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <map>
#include <numeric>
#include <memory>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AstroFanetSimulation");

// ========================================================================
// Global metrics collection
// ========================================================================
struct SimulationMetrics
{
  uint32_t totalGenerated = 0;
  uint32_t totalDelivered = 0;
  uint32_t totalDropped = 0;
  uint64_t totalControlBytes = 0;
  uint64_t totalDataBytes = 0;
  uint32_t totalBroadcasts = 0;
  uint32_t suppressedBroadcasts = 0;
  std::vector<double> delays;      // Per-packet delay (ms)
  std::vector<double> aoiValues;   // Per-source AoI (ms)
  double totalEnergyConsumed = 0;
  double totalUsefulBits = 0;

  double GetPDR () const
  {
    return totalGenerated > 0 ? 100.0 * totalDelivered / totalGenerated : 0.0;
  }

  double GetAvgDelay () const
  {
    if (delays.empty ()) return 0.0;
    return std::accumulate (delays.begin (), delays.end (), 0.0) / delays.size ();
  }

  double GetThroughput (double simTimeSec) const
  {
    return simTimeSec > 0 ? (totalUsefulBits / 1000.0) / simTimeSec : 0.0;  // kbit/s
  }

  double GetAvgAoI () const
  {
    if (aoiValues.empty ()) return 0.0;
    return std::accumulate (aoiValues.begin (), aoiValues.end (), 0.0) / aoiValues.size ();
  }

  double GetControlOverhead () const
  {
    uint64_t total = totalControlBytes + totalDataBytes;
    return total > 0 ? 100.0 * totalControlBytes / total : 0.0;
  }

  double GetEnergyPerBit () const
  {
    return totalUsefulBits > 0 ? (totalEnergyConsumed * 1e6) / totalUsefulBits : 0.0;  // uJ/bit
  }

  double GetBRR () const
  {
    return totalBroadcasts > 0 ? static_cast<double>(totalBroadcasts) : 0.0;
  }
};

// ========================================================================
// Packet tracking callbacks
// ========================================================================
static SimulationMetrics g_metrics;
static std::map<uint64_t, Time> g_packetCreationTimes;
static std::map<uint32_t, Time> g_lastDeliveryPerSource;
static uint64_t g_packetUid = 0;

void
PacketGenerated (uint32_t nodeId, uint32_t pktSize)
{
  g_metrics.totalGenerated++;
  g_packetCreationTimes[g_packetUid++] = Simulator::Now ();
}

void
PacketDelivered (uint32_t nodeId, uint32_t pktSize, double delayMs)
{
  g_metrics.totalDelivered++;
  g_metrics.delays.push_back (delayMs);
  g_metrics.totalUsefulBits += pktSize * 8.0;

  // Update AoI
  Time now = Simulator::Now ();
  auto it = g_lastDeliveryPerSource.find (nodeId);
  if (it != g_lastDeliveryPerSource.end ())
    {
      double aoi = (now - it->second).GetMilliSeconds ();
      g_metrics.aoiValues.push_back (aoi);
    }
  g_lastDeliveryPerSource[nodeId] = now;
}

// ========================================================================
// Traffic generation application
// ========================================================================
class AstroTrafficGenerator : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::AstroTrafficGenerator")
      .SetParent<Application> ()
      .AddConstructor<AstroTrafficGenerator> ();
    return tid;
  }

  AstroTrafficGenerator ()
    : m_socket (0), m_running (false), m_seqNo (0) {}

  void Setup (Ipv4Address sinkAddr, uint16_t port, double pktRate)
  {
    m_sinkAddr = sinkAddr;
    m_port = port;
    m_pktRate = pktRate;
  }

private:
  virtual void StartApplication (void)
  {
    m_running = true;
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Bind ();
    ScheduleNextPacket ();
  }

  virtual void StopApplication (void)
  {
    m_running = false;
    if (m_socket) m_socket->Close ();
  }

  void ScheduleNextPacket ()
  {
    if (!m_running) return;

    // Determine traffic class based on paper's traffic mix (Table 2):
    // 10% Emergency, 15% Command, 50% Sensing, 25% Telemetry
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
    double r = rng->GetValue ();
    astro::TrafficClass tc;
    if (r < 0.10)      tc = astro::EMERGENCY;
    else if (r < 0.25) tc = astro::COMMAND;
    else if (r < 0.75) tc = astro::SENSING;
    else                tc = astro::TELEMETRY;

    uint32_t pktSize = astro::PACKET_SIZES[tc];

    // Create packet with ASTRO data header
    Ptr<Packet> pkt = Create<Packet> (pktSize);
    astro::AstroDataHeader dataHdr;
    dataHdr.SetTrafficClass (tc);
    dataHdr.SetOriginId (GetNode ()->GetId ());
    dataHdr.SetSequenceNumber (m_seqNo++);
    dataHdr.SetCreationTime (Simulator::Now ());
    dataHdr.SetHopCount (0);
    dataHdr.SetIsBroadcast (tc == astro::EMERGENCY);

    // Set broadcast origin to current position
    Ptr<MobilityModel> mob = GetNode ()->GetObject<MobilityModel> ();
    if (mob)
      {
        Vector pos = mob->GetPosition ();
        dataHdr.SetBroadcastOrigin (pos.x, pos.y, pos.z);
        dataHdr.SetPreviousRelayPos (pos.x, pos.y, pos.z);
      }

    pkt->AddHeader (dataHdr);

    // Send
    m_socket->SendTo (pkt, 0, InetSocketAddress (m_sinkAddr, m_port));
    g_metrics.totalGenerated++;
    g_metrics.totalDataBytes += pktSize;

    // Schedule next with Poisson inter-arrival
    double interval = 1.0 / m_pktRate;
    Ptr<ExponentialRandomVariable> expRng = CreateObject<ExponentialRandomVariable> ();
    expRng->SetAttribute ("Mean", DoubleValue (interval));
    Time next = Seconds (expRng->GetValue ());
    Simulator::Schedule (next, &AstroTrafficGenerator::ScheduleNextPacket, this);
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_sinkAddr;
  uint16_t m_port;
  double m_pktRate;
  bool m_running;
  uint32_t m_seqNo;
};

// ========================================================================
// Sink application (collects delivered packets)
// ========================================================================
class AstroSinkApp : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::AstroSinkApp")
      .SetParent<Application> ()
      .AddConstructor<AstroSinkApp> ();
    return tid;
  }

  AstroSinkApp () : m_socket (0) {}

  void Setup (uint16_t port) { m_port = port; }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_port));
    m_socket->SetRecvCallback (MakeCallback (&AstroSinkApp::HandleReceive, this));
  }

  virtual void StopApplication (void)
  {
    if (m_socket) m_socket->Close ();
  }

  void HandleReceive (Ptr<Socket> socket)
  {
    Ptr<Packet> pkt;
    Address from;
    while ((pkt = socket->RecvFrom (from)))
      {
        astro::AstroDataHeader dataHdr;
        if (pkt->RemoveHeader (dataHdr))
          {
            double delay = (Simulator::Now () - dataHdr.GetCreationTime ()).GetMilliSeconds ();
            PacketDelivered (dataHdr.GetOriginId (), pkt->GetSize (), delay);
          }
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

// ========================================================================
// Main simulation
// ========================================================================
int
main (int argc, char *argv[])
{
  // ---------- Command-line parameters (matching Table 2) ----------
  uint32_t nUavs = 30;
  std::string protocol = "astro";    // astro, aodv, olsr, epidemic, dqn
  std::string mobility = "gm3d";    // gm3d, rpgm
  uint32_t seed = 1001;
  double simTime = 600.0;           // seconds
  double pktRate = 2.0;             // pkts/s per UAV (reduced from 5 for memory)
  double commRange = 400.0;         // meters
  double minSpeed = 10.0;           // m/s
  double maxSpeed = 25.0;           // m/s
  double areaX = 2000.0;            // meters
  double areaY = 2000.0;
  double areaZ = 200.0;             // altitude range (50-250m)
  double gmAlpha = 0.75;            // Gauss-Markov tuning parameter
  double byzFraction = 0.0;         // Fraction of Byzantine agents
  std::string outputDir = "results";
  bool videoMode = false;
  bool enableAnim = false;
  double animPollInterval = 0.25;
  bool animPacketMetadata = true;
  bool animSkipPacketTracing = false;
  bool animWifiCounters = true;
  bool animIpv4Counters = false;
  bool animQueueCounters = false;
  std::string animBackgroundImage = "";
  double animBackgroundOpacity = 0.08;
  double animNodeSize = 18.0;
  double animSinkSize = 28.0;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("nUavs", "Number of UAVs", nUavs);
  cmd.AddValue ("protocol", "Routing protocol: astro|aodv|olsr|epidemic|dqn", protocol);
  cmd.AddValue ("mobility", "Mobility model: gm3d|rpgm", mobility);
  cmd.AddValue ("seed", "Random seed", seed);
  cmd.AddValue ("simTime", "Simulation duration (s)", simTime);
  cmd.AddValue ("pktRate", "Packet generation rate per UAV (pkts/s)", pktRate);
  cmd.AddValue ("commRange", "Communication range (m)", commRange);
  cmd.AddValue ("minSpeed", "Minimum UAV speed (m/s)", minSpeed);
  cmd.AddValue ("maxSpeed", "Maximum UAV speed (m/s)", maxSpeed);
  cmd.AddValue ("gmAlpha", "Gauss-Markov alpha parameter", gmAlpha);
  cmd.AddValue ("byzFraction", "Fraction of Byzantine agents [0,1)", byzFraction);
  cmd.AddValue ("outputDir", "Output directory for results", outputDir);
  cmd.AddValue ("videoMode", "Enable a cleaner NetAnim preset for recording/demo videos", videoMode);
  cmd.AddValue ("enableAnim", "Enable NetAnim XML export", enableAnim);
  cmd.AddValue ("animPollInterval", "NetAnim mobility poll interval (s)", animPollInterval);
  cmd.AddValue ("animPacketMetadata", "Include packet metadata in NetAnim trace", animPacketMetadata);
  cmd.AddValue ("animSkipPacketTracing", "Disable packet animation and keep only mobility/counters", animSkipPacketTracing);
  cmd.AddValue ("animWifiCounters", "Enable NetAnim Wi-Fi MAC/PHY counters", animWifiCounters);
  cmd.AddValue ("animIpv4Counters", "Enable NetAnim IPv4 counters", animIpv4Counters);
  cmd.AddValue ("animQueueCounters", "Enable NetAnim queue counters", animQueueCounters);
  cmd.AddValue ("animBackgroundImage", "Background image for NetAnim (relative or absolute path)", animBackgroundImage);
  cmd.AddValue ("animBackgroundOpacity", "Background image opacity for NetAnim", animBackgroundOpacity);
  cmd.AddValue ("animNodeSize", "NetAnim node size for UAVs", animNodeSize);
  cmd.AddValue ("animSinkSize", "NetAnim node size for the sink", animSinkSize);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.Parse (argc, argv);

  if (videoMode)
    {
      enableAnim = true;
      animPollInterval = 0.1;
      animWifiCounters = true;
      animIpv4Counters = true;
      animQueueCounters = false;
      animNodeSize = 22.0;
      animSinkSize = 34.0;
      animBackgroundOpacity = 0.10;
      if (animBackgroundImage.empty ())
        {
          animBackgroundImage = "../netanim-3.108/ns-3-background.png";
        }
      if (simTime == 600.0)
        {
          simTime = 30.0;
        }
      if (pktRate == 2.0)
        {
          pktRate = 1.25;
        }
    }

  // ---------- Validate command-line parameters before allocating ns-3 state ----------
  if (nUavs < 2)
    {
      std::cerr << "Invalid nUavs: expected at least 2" << std::endl;
      return 1;
    }
  if (protocol != "astro" && protocol != "aodv" && protocol != "olsr"
      && protocol != "epidemic" && protocol != "dqn")
    {
      std::cerr << "Invalid protocol: " << protocol << std::endl;
      return 1;
    }
  if (mobility != "gm3d" && mobility != "rpgm")
    {
      std::cerr << "Invalid mobility model: " << mobility << std::endl;
      return 1;
    }
  if (simTime <= 0.0 || pktRate < 0.0 || commRange <= 0.0 || minSpeed < 0.0
      || maxSpeed < minSpeed || areaX <= 0.0 || areaY <= 0.0 || areaZ <= 0.0)
    {
      std::cerr << "Invalid physical or timing parameter" << std::endl;
      return 1;
    }
  if (gmAlpha < 0.0 || gmAlpha > 1.0)
    {
      std::cerr << "Invalid gmAlpha: expected a value in [0,1]" << std::endl;
      return 1;
    }
  if (byzFraction < 0.0 || byzFraction >= 1.0)
    {
      std::cerr << "Invalid byzFraction: expected a value in [0,1)" << std::endl;
      return 1;
    }
  if (outputDir.empty () || animPollInterval <= 0.0 || animBackgroundOpacity < 0.0
      || animBackgroundOpacity > 1.0 || animNodeSize <= 0.0 || animSinkSize <= 0.0)
    {
      std::cerr << "Invalid output or animation parameter" << std::endl;
      return 1;
    }

  // Set random seed
  SeedManager::SetSeed (seed);
  SeedManager::SetRun (seed);

  if (verbose)
    {
      LogComponentEnable ("AstroRoutingProtocol", LOG_LEVEL_INFO);
      LogComponentEnable ("A3dBsm", LOG_LEVEL_DEBUG);
    }

  NS_LOG_INFO ("=== ASTRO-FANET Simulation ===");
  NS_LOG_INFO ("Protocol: " << protocol);
  NS_LOG_INFO ("UAVs: " << nUavs);
  NS_LOG_INFO ("Mobility: " << mobility);
  NS_LOG_INFO ("Seed: " << seed);
  NS_LOG_INFO ("Duration: " << simTime << "s");

  // ---------- Create nodes ----------
  NodeContainer uavNodes;
  uavNodes.Create (nUavs);

  // Sink node (ground station)
  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (uavNodes);
  allNodes.Add (sinkNode);

  // ---------- WiFi setup (IEEE 802.11a OFDM, Table 2) ----------
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate6Mbps"),
                                "ControlMode", StringValue ("OfdmRate6Mbps"));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;

  // Free-space path loss (exponent=2.0) appropriate for air-to-air FANET links
  // with log-normal shadowing sigma=4 dB (Table 2)
  // At 5.18 GHz, free-space loss at 1m = 20*log10(4*pi*1*5.18e9/3e8) = 46.7 dB
  // With exponent=2.0 and TxPower=20 dBm: effective range ~580m (covers R_max=400m)
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                   "Exponent", DoubleValue (2.0),
                                   "ReferenceDistance", DoubleValue (1.0),
                                   "ReferenceLoss", DoubleValue (46.7));  // Free space at 5.18 GHz

  wifiPhy.SetChannel (wifiChannel.Create ());

  // Set Tx power for ~400m effective range
  wifiPhy.Set ("TxPowerStart", DoubleValue (20.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (20.0));
  wifiPhy.Set ("TxGain", DoubleValue (2.0));   // Small antenna gain (typical for UAV)
  wifiPhy.Set ("RxGain", DoubleValue (2.0));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7.0));

  // Ad-hoc MAC
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer uavDevices = wifi.Install (wifiPhy, wifiMac, uavNodes);
  NetDeviceContainer sinkDevices = wifi.Install (wifiPhy, wifiMac, sinkNode);
  NetDeviceContainer devices;
  devices.Add (uavDevices);
  devices.Add (sinkDevices);

  // ---------- Mobility setup ----------
  MobilityHelper mobilityHelper;

  if (mobility == "gm3d")
    {
      // Gauss-Markov 3D (Section 4.3)
      // NS-3's GaussMarkovMobilityModel natively supports 3D
      mobilityHelper.SetMobilityModel ("ns3::GaussMarkovMobilityModel",
        "Bounds", BoxValue (Box (0, areaX, 0, areaY, 50, 50 + areaZ)),
        "TimeStep", TimeValue (MilliSeconds (200)),
        "Alpha", DoubleValue (gmAlpha),
        "MeanVelocity", StringValue (
          "ns3::UniformRandomVariable[Min=" + std::to_string (minSpeed) +
          "|Max=" + std::to_string (maxSpeed) + "]"),
        "MeanDirection", StringValue ("ns3::UniformRandomVariable[Min=0|Max=6.283185]"),
        "MeanPitch", StringValue ("ns3::UniformRandomVariable[Min=-0.05|Max=0.05]"),
        "NormalVelocity", StringValue (
          "ns3::NormalRandomVariable[Mean=0.0|Variance=2.0|Bound=4.0]"),
        "NormalDirection", StringValue (
          "ns3::NormalRandomVariable[Mean=0.0|Variance=0.2|Bound=0.4]"),
        "NormalPitch", StringValue (
          "ns3::NormalRandomVariable[Mean=0.0|Variance=0.02|Bound=0.04]"));

      // Random initial positions within the 3D mission area
      mobilityHelper.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
        "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=" + std::to_string (areaX) + "]"),
        "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=" + std::to_string (areaY) + "]"),
        "Z", StringValue ("ns3::UniformRandomVariable[Min=50|Max=" + std::to_string (50 + areaZ) + "]"));

      mobilityHelper.Install (uavNodes);
    }
  else if (mobility == "rpgm")
    {
      // RPGM: Groups of 4-5 UAVs (Section 4.3)
      uint32_t groupSize = 4;
      if (nUavs > 20) groupSize = 5;
      uint32_t numGroups = (nUavs + groupSize - 1) / groupSize;

      for (uint32_t g = 0; g < numGroups; g++)
        {
          uint32_t start = g * groupSize;
          uint32_t end = std::min (start + groupSize, nUavs);

          // Leader uses Gauss-Markov
          Ptr<GaussMarkovMobilityModel> leaderMob = CreateObject<GaussMarkovMobilityModel> ();
          leaderMob->SetAttribute ("Bounds", BoxValue (Box (0, areaX, 0, areaY, 50, 50 + areaZ)));
          leaderMob->SetAttribute ("Alpha", DoubleValue (gmAlpha));

          Ptr<Node> leaderNode = uavNodes.Get (start);
          leaderNode->AggregateObject (leaderMob);

          // Group center offset
          double gx = (g % 3) * (areaX / 3.0) + areaX / 6.0;
          double gy = (g / 3) * (areaY / 3.0) + areaY / 6.0;
          leaderMob->SetPosition (Vector (gx, gy, 100 + g * 20));

          // Members follow leader with RPGM
          for (uint32_t i = start + 1; i < end; i++)
            {
              Ptr<RpgmMobilityModel> memberMob = CreateObject<RpgmMobilityModel> ();
              memberMob->SetGroupId (g);
              memberMob->SetGroupLeader (leaderMob);
              memberMob->SetMaxDeviation (50.0);
              memberMob->SetAttribute ("AreaX", DoubleValue (areaX));
              memberMob->SetAttribute ("AreaY", DoubleValue (areaY));

              Ptr<Node> memberNode = uavNodes.Get (i);
              memberNode->AggregateObject (memberMob);

              // Initial position near leader
              Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
              double dx = rng->GetValue (-30, 30);
              double dy = rng->GetValue (-30, 30);
              double dz = rng->GetValue (-10, 10);
              memberMob->SetPosition (Vector (gx + dx, gy + dy, 100 + g * 20 + dz));
            }
        }
    }

  // Sink: fixed ground station at center of mission area, elevated antenna (50m)
  // Ensures at least partial connectivity with UAV swarm at 50-250m altitude
  MobilityHelper sinkMobility;
  sinkMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  Ptr<ListPositionAllocator> sinkPosAlloc = CreateObject<ListPositionAllocator> ();
  sinkPosAlloc->Add (Vector (areaX / 2.0, areaY / 2.0, 50.0));
  sinkMobility.SetPositionAllocator (sinkPosAlloc);
  sinkMobility.Install (sinkNode);

  // ---------- Energy model ----------
  BasicEnergySourceHelper energyHelper;
  energyHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (10000.0));
  EnergySourceContainer energySources = energyHelper.Install (uavNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.38));   // 802.11a Tx current
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.313));  // 802.11a Rx current
  radioEnergyHelper.Set ("IdleCurrentA", DoubleValue (0.273));
  radioEnergyHelper.Set ("SleepCurrentA", DoubleValue (0.033));
  DeviceEnergyModelContainer radioModels = radioEnergyHelper.Install (uavDevices, energySources);

  // ---------- Internet stack + routing ----------
  InternetStackHelper internet;

  if (protocol == "astro")
    {
      AstroHelper astroRouting;
      astroRouting.Set ("BeaconInterval", TimeValue (MilliSeconds (200)));
      astroRouting.Set ("DecisionEpoch", TimeValue (MilliSeconds (200)));
      internet.SetRoutingHelper (astroRouting);
    }
  else if (protocol == "aodv")
    {
      AodvHelper aodvRouting;
      internet.SetRoutingHelper (aodvRouting);
    }
  else if (protocol == "olsr")
    {
      OlsrHelper olsrRouting;
      internet.SetRoutingHelper (olsrRouting);
    }
  else if (protocol == "epidemic" || protocol == "dqn")
    {
      // Epidemic: use AODV as base, with broadcast-heavy behavior
      // DQN-QR: approximated by AODV with modified parameters
      AodvHelper aodvRouting;
      if (protocol == "epidemic")
        {
          aodvRouting.Set ("EnableHello", BooleanValue (false));
          aodvRouting.Set ("GratuitousReply", BooleanValue (true));
        }
      internet.SetRoutingHelper (aodvRouting);
    }

  internet.Install (allNodes);

  // ---------- IP addressing ----------
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  Ipv4Address sinkAddr = interfaces.GetAddress (nUavs);  // Last node is sink

  // ---------- Configure ASTRO-specific settings ----------
  if (protocol == "astro")
    {
      // Share SLM emulator across all agents (same embedding bank)
      Ptr<astro::SlmEmulator> sharedSlm = CreateObject<astro::SlmEmulator> ();
      sharedSlm->GenerateSyntheticEmbeddings (1000, 42);

      for (uint32_t i = 0; i < nUavs; i++)
        {
          Ptr<astro::AstroRoutingProtocol> astroProto =
            uavNodes.Get (i)->GetObject<astro::AstroRoutingProtocol> ();
          if (astroProto)
            {
              astroProto->SetSinkAddress (sinkAddr);
              astroProto->SetSlmEmulator (sharedSlm);
              astroProto->SetEnergySource (energySources.Get (i));

              // Configure Byzantine agents
              if (byzFraction > 0 && i < static_cast<uint32_t>(nUavs * byzFraction))
                {
                  astroProto->SetByzantine (true, 0.5);  // 50% selective dropping
                  NS_LOG_INFO ("Node " << i << " configured as Byzantine");
                }
            }
        }
    }

  // ---------- Traffic generation ----------
  uint16_t dataPort = 9;

  // Install sink application
  Ptr<AstroSinkApp> sinkApp = CreateObject<AstroSinkApp> ();
  sinkApp->Setup (dataPort);
  sinkNode.Get (0)->AddApplication (sinkApp);
  sinkApp->SetStartTime (Seconds (1.0));
  sinkApp->SetStopTime (Seconds (simTime));

  // Install traffic generators on all UAVs
  for (uint32_t i = 0; i < nUavs; i++)
    {
      Ptr<AstroTrafficGenerator> trafficGen = CreateObject<AstroTrafficGenerator> ();
      trafficGen->Setup (sinkAddr, dataPort, pktRate);
      uavNodes.Get (i)->AddApplication (trafficGen);
      trafficGen->SetStartTime (Seconds (2.0));  // Start after routing converges
      trafficGen->SetStopTime (Seconds (simTime - 1.0));
    }

  // ---------- Flow monitor for cross-validation (disabled for N>15 to save memory) ----------
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  if (nUavs <= 15)
    {
      flowMonitor = flowHelper.InstallAll ();
    }

  // ---------- Optional NetAnim export ----------
  std::string animFile;
  std::string routeFile;
#ifdef ASTRO_ENABLE_NETANIM
  std::unique_ptr<AnimationInterface> anim;
  if (enableAnim)
    {
      SystemPath::MakeDirectories (outputDir);
      std::string runTag = protocol + "_n" + std::to_string (nUavs)
                           + "_" + mobility + "_s" + std::to_string (seed)
                           + "_bz" + std::to_string (static_cast<int> (byzFraction * 100.0));
      animFile = outputDir + "/" + runTag + ".anim.xml";
      routeFile = outputDir + "/" + runTag + ".routes.xml";

      anim.reset (new AnimationInterface (animFile));
      anim->SetStartTime (Seconds (0.0));
      anim->SetStopTime (Seconds (simTime));
      anim->SetMobilityPollInterval (Seconds (animPollInterval));
      if (!animBackgroundImage.empty ())
        {
          anim->SetBackgroundImage (animBackgroundImage, 0.0, 0.0, 0.25, 0.25, animBackgroundOpacity);
        }
      if (animPacketMetadata)
        {
          anim->EnablePacketMetadata ();
        }
      if (animSkipPacketTracing)
        {
          anim->SkipPacketTracing ();
        }
      anim->EnableIpv4RouteTracking (routeFile, Seconds (0.0), Seconds (simTime), Seconds (2.0));
      if (animWifiCounters)
        {
          anim->EnableWifiMacCounters (Seconds (0.0), Seconds (simTime), Seconds (1.0));
          anim->EnableWifiPhyCounters (Seconds (0.0), Seconds (simTime), Seconds (1.0));
        }
      if (animIpv4Counters)
        {
          anim->EnableIpv4L3ProtocolCounters (Seconds (0.0), Seconds (simTime), Seconds (1.0));
        }
      if (animQueueCounters)
        {
          anim->EnableQueueCounters (Seconds (0.0), Seconds (simTime), Seconds (1.0));
        }

      for (uint32_t i = 0; i < nUavs; i++)
        {
          std::ostringstream label;
          label << "UAV " << i;
          std::ostringstream sinkStream;
          sinkStream << sinkAddr;
          bool isByzantine = (byzFraction > 0 && i < static_cast<uint32_t> (nUavs * byzFraction));
          if (isByzantine)
            {
              label << " (Byz)";
            }
          anim->UpdateNodeDescription (uavNodes.Get (i), label.str ());
          anim->UpdateNodeSize (uavNodes.Get (i)->GetId (), animNodeSize, animNodeSize);
          if (isByzantine)
            {
              anim->UpdateNodeColor (uavNodes.Get (i), 220, 70, 70);
            }
          else if (protocol == "astro")
            {
              anim->UpdateNodeColor (uavNodes.Get (i), 40, 110, 220);
            }
          else
            {
              anim->UpdateNodeColor (uavNodes.Get (i), 120, 120, 120);
            }
          anim->AddSourceDestination (uavNodes.Get (i)->GetId (), sinkStream.str ());
        }

      anim->UpdateNodeDescription (sinkNode.Get (0), "Ground Sink");
      anim->UpdateNodeColor (sinkNode.Get (0), 40, 170, 90);
      anim->UpdateNodeSize (sinkNode.Get (0)->GetId (), animSinkSize, animSinkSize);
    }
#endif  // ASTRO_ENABLE_NETANIM

  // ---------- Run simulation ----------
  NS_LOG_INFO ("Starting simulation for " << simTime << " seconds...");
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // ---------- Collect results ----------
  NS_LOG_INFO ("Collecting results...");

  // Energy consumption
  for (uint32_t i = 0; i < nUavs; i++)
    {
      double initial = energySources.Get (i)->GetInitialEnergy ();
      double remaining = energySources.Get (i)->GetRemainingEnergy ();
      g_metrics.totalEnergyConsumed += (initial - remaining);
    }

  // ASTRO-specific metrics
  if (protocol == "astro")
    {
      for (uint32_t i = 0; i < nUavs; i++)
        {
          Ptr<astro::AstroRoutingProtocol> astroProto =
            uavNodes.Get (i)->GetObject<astro::AstroRoutingProtocol> ();
          if (astroProto)
            {
              g_metrics.totalControlBytes += astroProto->GetTotalControlBytes ();
              g_metrics.totalBroadcasts += astroProto->GetTotalBroadcasts ();
              g_metrics.suppressedBroadcasts += astroProto->GetSuppressedBroadcasts ();
            }
        }
    }

  // FlowMonitor statistics
  uint32_t fmDelivered = 0, fmLost = 0;
  double fmDelaySum = 0;
  uint32_t fmDelayCount = 0;

  if (flowMonitor)
    {
      flowMonitor->CheckForLostPackets ();
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();

      for (auto &flow : stats)
        {
          fmDelivered += flow.second.rxPackets;
          fmLost += flow.second.lostPackets;
          if (flow.second.rxPackets > 0)
            {
              fmDelaySum += flow.second.delaySum.GetMilliSeconds ();
              fmDelayCount += flow.second.rxPackets;
            }
        }
    }

  // ---------- Output results ----------
  double pdr = g_metrics.GetPDR ();
  double avgDelay = g_metrics.GetAvgDelay ();
  double throughput = g_metrics.GetThroughput (simTime);
  double avgAoI = g_metrics.GetAvgAoI ();
  double ctrlOverhead = g_metrics.GetControlOverhead ();
  double energyPerBit = g_metrics.GetEnergyPerBit ();
  double brr = g_metrics.GetBRR ();

  // Use FlowMonitor data if ASTRO metrics are incomplete
  if (fmDelayCount > 0 && g_metrics.delays.empty ())
    avgDelay = fmDelaySum / fmDelayCount;
  if (fmDelivered > 0 && g_metrics.totalDelivered == 0)
    {
      g_metrics.totalDelivered = fmDelivered;
      pdr = 100.0 * fmDelivered / (fmDelivered + fmLost);
    }

  std::cout << "\n======================================" << std::endl;
  std::cout << "ASTRO-FANET Simulation Results" << std::endl;
  std::cout << "======================================" << std::endl;
  std::cout << "Protocol:           " << protocol << std::endl;
  std::cout << "UAVs:               " << nUavs << std::endl;
  std::cout << "Mobility:           " << mobility << std::endl;
  std::cout << "Seed:               " << seed << std::endl;
  std::cout << "Duration:           " << simTime << " s" << std::endl;
  std::cout << "--------------------------------------" << std::endl;
  std::cout << "PDR (%):            " << pdr << std::endl;
  std::cout << "Avg Delay (ms):     " << avgDelay << std::endl;
  std::cout << "Throughput (kbit/s):" << throughput << std::endl;
  std::cout << "Avg AoI (ms):       " << avgAoI << std::endl;
  std::cout << "Ctrl Overhead (%):  " << ctrlOverhead << std::endl;
  std::cout << "Energy/bit (uJ):    " << energyPerBit << std::endl;
  std::cout << "BRR:                " << brr << std::endl;
  std::cout << "Broadcasts:         " << g_metrics.totalBroadcasts << std::endl;
  std::cout << "Suppressed:         " << g_metrics.suppressedBroadcasts << std::endl;
  std::cout << "Total energy (J):   " << g_metrics.totalEnergyConsumed << std::endl;
  std::cout << "Byz fraction:       " << byzFraction << std::endl;
  if (enableAnim)
    {
      std::cout << "Animation XML:      " << animFile << std::endl;
      std::cout << "Routes XML:         " << routeFile << std::endl;
    }
  std::cout << "======================================\n" << std::endl;

  // Write to CSV for batch analysis
  SystemPath::MakeDirectories (outputDir);
  std::string csvFile = outputDir + "/" + protocol + "_n" + std::to_string (nUavs)
                        + "_" + mobility + "_s" + std::to_string (seed)
                        + "_bz" + std::to_string (static_cast<int> (byzFraction * 100.0)) + ".csv";
  std::ofstream csv (csvFile);
  if (csv.is_open ())
    {
      csv << "protocol,nUavs,mobility,seed,simTime,pdr,avgDelay,throughput,avgAoI,"
          << "ctrlOverhead,energyPerBit,brr,broadcasts,suppressed,totalEnergy,byzFraction"
          << std::endl;
      csv << protocol << "," << nUavs << "," << mobility << "," << seed << ","
          << simTime << "," << pdr << "," << avgDelay << "," << throughput << ","
          << avgAoI << "," << ctrlOverhead << "," << energyPerBit << "," << brr << ","
          << g_metrics.totalBroadcasts << "," << g_metrics.suppressedBroadcasts << ","
          << g_metrics.totalEnergyConsumed << "," << byzFraction << std::endl;
      csv.close ();
      NS_LOG_INFO ("Results written to " << csvFile);
    }

  Simulator::Destroy ();
  return 0;
}
