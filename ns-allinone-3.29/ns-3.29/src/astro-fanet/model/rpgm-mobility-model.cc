/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "rpgm-mobility-model.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RpgmMobilityModel");
NS_OBJECT_ENSURE_REGISTERED (RpgmMobilityModel);

TypeId
RpgmMobilityModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RpgmMobilityModel")
    .SetParent<MobilityModel> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<RpgmMobilityModel> ()
    .AddAttribute ("MaxDeviation",
                   "Maximum deviation from group leader (meters)",
                   DoubleValue (50.0),
                   MakeDoubleAccessor (&RpgmMobilityModel::m_maxDeviation),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("UpdateInterval",
                   "Position update interval",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&RpgmMobilityModel::m_updateInterval),
                   MakeTimeChecker ())
    .AddAttribute ("AreaX", "Mission area X dimension (m)",
                   DoubleValue (2000.0),
                   MakeDoubleAccessor (&RpgmMobilityModel::m_areaX),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("AreaY", "Mission area Y dimension (m)",
                   DoubleValue (2000.0),
                   MakeDoubleAccessor (&RpgmMobilityModel::m_areaY),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MinAltitude", "Minimum altitude (m)",
                   DoubleValue (50.0),
                   MakeDoubleAccessor (&RpgmMobilityModel::m_minAlt),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxAltitude", "Maximum altitude (m)",
                   DoubleValue (250.0),
                   MakeDoubleAccessor (&RpgmMobilityModel::m_maxAlt),
                   MakeDoubleChecker<double> ())
    ;
  return tid;
}

RpgmMobilityModel::RpgmMobilityModel ()
  : m_groupId (0),
    m_isLeader (false),
    m_maxDeviation (50.0),
    m_areaX (2000.0), m_areaY (2000.0),
    m_minAlt (50.0), m_maxAlt (250.0),
    m_leaderSpeed (15.0),
    m_lastUpdate (Seconds (0))
{
  m_rng = CreateObject<UniformRandomVariable> ();
  m_speedRng = CreateObject<UniformRandomVariable> ();
  m_speedRng->SetAttribute ("Min", DoubleValue (10.0));
  m_speedRng->SetAttribute ("Max", DoubleValue (25.0));

  m_currentPosition = Vector (0, 0, 100);
  m_currentVelocity = Vector (0, 0, 0);
  m_referencePoint = Vector (0, 0, 100);
  m_leaderTarget = Vector (1000, 1000, 150);
}

RpgmMobilityModel::~RpgmMobilityModel ()
{
}

void
RpgmMobilityModel::SetGroupLeader (Ptr<MobilityModel> leader)
{
  m_leaderMobility = leader;
}

void
RpgmMobilityModel::SetMaxDeviation (double maxDev)
{
  m_maxDeviation = maxDev;
}

void
RpgmMobilityModel::SetGroupId (uint32_t groupId)
{
  m_groupId = groupId;
}

uint32_t
RpgmMobilityModel::GetGroupId () const
{
  return m_groupId;
}

void
RpgmMobilityModel::SetIsLeader (bool isLeader)
{
  m_isLeader = isLeader;
}

bool
RpgmMobilityModel::IsLeader () const
{
  return m_isLeader;
}

Vector
RpgmMobilityModel::PickRandomWaypoint () const
{
  double x = m_rng->GetValue (0.0, m_areaX);
  double y = m_rng->GetValue (0.0, m_areaY);
  double z = m_rng->GetValue (m_minAlt, m_maxAlt);
  return Vector (x, y, z);
}

void
RpgmMobilityModel::Update (void) const
{
  Time now = Simulator::Now ();
  double dt = (now - m_lastUpdate).GetSeconds ();
  if (dt <= 0)
    return;

  if (m_isLeader)
    {
      // Leader: move toward target waypoint
      Vector diff (m_leaderTarget.x - m_currentPosition.x,
                   m_leaderTarget.y - m_currentPosition.y,
                   m_leaderTarget.z - m_currentPosition.z);
      double dist = std::sqrt (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

      if (dist < 10.0)
        {
          // Arrived at waypoint, pick new one
          m_leaderTarget = PickRandomWaypoint ();
          m_leaderSpeed = m_speedRng->GetValue ();
          diff = Vector (m_leaderTarget.x - m_currentPosition.x,
                         m_leaderTarget.y - m_currentPosition.y,
                         m_leaderTarget.z - m_currentPosition.z);
          dist = std::sqrt (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        }

      if (dist > 0)
        {
          m_currentVelocity = Vector (diff.x / dist * m_leaderSpeed,
                                      diff.y / dist * m_leaderSpeed,
                                      diff.z / dist * m_leaderSpeed);
        }

      m_currentPosition.x += m_currentVelocity.x * dt;
      m_currentPosition.y += m_currentVelocity.y * dt;
      m_currentPosition.z += m_currentVelocity.z * dt;

      // Boundary reflection
      if (m_currentPosition.x < 0 || m_currentPosition.x > m_areaX)
        m_currentVelocity.x = -m_currentVelocity.x;
      if (m_currentPosition.y < 0 || m_currentPosition.y > m_areaY)
        m_currentVelocity.y = -m_currentVelocity.y;
      if (m_currentPosition.z < m_minAlt || m_currentPosition.z > m_maxAlt)
        m_currentVelocity.z = -m_currentVelocity.z;

      m_currentPosition.x = std::max (0.0, std::min (m_areaX, m_currentPosition.x));
      m_currentPosition.y = std::max (0.0, std::min (m_areaY, m_currentPosition.y));
      m_currentPosition.z = std::max (m_minAlt, std::min (m_maxAlt, m_currentPosition.z));
    }
  else
    {
      // Member: follow leader reference point with bounded deviation
      if (m_leaderMobility)
        {
          Vector leaderPos = m_leaderMobility->GetPosition ();
          m_referencePoint = leaderPos;

          // Add bounded random deviation
          double devX = m_rng->GetValue (-m_maxDeviation, m_maxDeviation);
          double devY = m_rng->GetValue (-m_maxDeviation, m_maxDeviation);
          double devZ = m_rng->GetValue (-m_maxDeviation * 0.3, m_maxDeviation * 0.3);

          Vector target (m_referencePoint.x + devX,
                         m_referencePoint.y + devY,
                         m_referencePoint.z + devZ);

          // Smooth movement toward target
          double smoothing = 0.3;
          m_currentVelocity.x = smoothing * (target.x - m_currentPosition.x) / dt;
          m_currentVelocity.y = smoothing * (target.y - m_currentPosition.y) / dt;
          m_currentVelocity.z = smoothing * (target.z - m_currentPosition.z) / dt;

          // Cap speed
          double speed = std::sqrt (m_currentVelocity.x * m_currentVelocity.x +
                                     m_currentVelocity.y * m_currentVelocity.y +
                                     m_currentVelocity.z * m_currentVelocity.z);
          double maxSpeed = 25.0;
          if (speed > maxSpeed)
            {
              m_currentVelocity.x *= maxSpeed / speed;
              m_currentVelocity.y *= maxSpeed / speed;
              m_currentVelocity.z *= maxSpeed / speed;
            }

          m_currentPosition.x += m_currentVelocity.x * dt;
          m_currentPosition.y += m_currentVelocity.y * dt;
          m_currentPosition.z += m_currentVelocity.z * dt;

          // Enforce bounds
          m_currentPosition.x = std::max (0.0, std::min (m_areaX, m_currentPosition.x));
          m_currentPosition.y = std::max (0.0, std::min (m_areaY, m_currentPosition.y));
          m_currentPosition.z = std::max (m_minAlt, std::min (m_maxAlt, m_currentPosition.z));
        }
    }

  m_lastUpdate = now;
}

Vector
RpgmMobilityModel::DoGetPosition (void) const
{
  Update ();
  return m_currentPosition;
}

void
RpgmMobilityModel::DoSetPosition (const Vector &position)
{
  m_currentPosition = position;
  m_referencePoint = position;
  m_lastUpdate = Simulator::Now ();
  NotifyCourseChange ();
}

Vector
RpgmMobilityModel::DoGetVelocity (void) const
{
  return m_currentVelocity;
}

} // namespace ns3
