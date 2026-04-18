/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Reference Point Group Mobility (RPGM) Model for FANETs
 *
 * Groups of 4-5 UAVs follow a group leader with bounded deviation.
 * Models coordinated search-and-rescue or agricultural inspection missions.
 * See Section 4.3 of the paper.
 */
#ifndef RPGM_MOBILITY_MODEL_H
#define RPGM_MOBILITY_MODEL_H

#include "ns3/mobility-model.h"
#include "ns3/random-variable-stream.h"
#include "ns3/nstime.h"
#include "ns3/vector.h"
#include "ns3/event-id.h"

namespace ns3 {

/**
 * \brief Reference Point Group Mobility model
 *
 * Each node belongs to a group. The group leader moves according to
 * a random waypoint model within the 3D mission area. Group members
 * maintain a reference point that follows the leader with bounded
 * random deviation.
 */
class RpgmMobilityModel : public MobilityModel
{
public:
  static TypeId GetTypeId (void);

  RpgmMobilityModel ();
  virtual ~RpgmMobilityModel ();

  /**
   * Set the group leader's mobility model. Group members follow this leader.
   */
  void SetGroupLeader (Ptr<MobilityModel> leader);

  /**
   * Set the maximum deviation from the group leader (in meters).
   */
  void SetMaxDeviation (double maxDev);

  /**
   * Set the group ID for this node.
   */
  void SetGroupId (uint32_t groupId);
  uint32_t GetGroupId () const;

  /**
   * Is this node a group leader?
   */
  void SetIsLeader (bool isLeader);
  bool IsLeader () const;

private:
  virtual Vector DoGetPosition (void) const;
  virtual void DoSetPosition (const Vector &position);
  virtual Vector DoGetVelocity (void) const;

  void Update (void) const;
  void ScheduleUpdate (void);

  // Leader movement (random waypoint in 3D)
  void LeaderUpdate (void);
  Vector PickRandomWaypoint (void) const;

  Ptr<MobilityModel> m_leaderMobility;
  uint32_t m_groupId;
  bool m_isLeader;

  double m_maxDeviation;     // Max deviation from reference point (m)
  Time m_updateInterval;     // Position update interval

  // 3D mission area bounds
  double m_areaX, m_areaY, m_areaZ;   // Max coordinates
  double m_minAlt, m_maxAlt;           // Altitude range

  // Leader waypoint state
  mutable Vector m_leaderTarget;
  mutable double m_leaderSpeed;

  // Member state
  mutable Vector m_referencePoint;     // Reference point (tracks leader)
  mutable Vector m_currentPosition;
  mutable Vector m_currentVelocity;
  mutable Time m_lastUpdate;

  // Random variables
  Ptr<UniformRandomVariable> m_rng;
  Ptr<UniformRandomVariable> m_speedRng;

  EventId m_updateEvent;
};

} // namespace ns3

#endif /* RPGM_MOBILITY_MODEL_H */
