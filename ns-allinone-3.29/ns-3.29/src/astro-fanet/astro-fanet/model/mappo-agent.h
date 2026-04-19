/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET legacy route-selection helper.
 *
 * This class is retained by the executable ns-3 scaffold for next-hop
 * selection compatibility. It is not used to calibrate the A3D-BSM
 * suppression rule described in the current manuscript.
 *
 * Architecture:
 * - Actor: 2-layer MLP [256, 128] with pointer network head for neighbor selection
 * - Input: observation o_i(t) = [z_i(t) || mean(iota_neighbors)]
 * - Output: action a_i(t) from the feasible action set A_i(t)
 *
 * This implementation supports:
 * 1. Loading pre-trained weights from file (for deployment)
 * 2. Random policy (for baseline comparison)
 * 3. Interface for external Python training via file-based communication
 */
#ifndef MAPPO_AGENT_H
#define MAPPO_AGENT_H

#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "astro-packet.h"
#include <vector>
#include <map>
#include <string>

namespace ns3 {
namespace astro {

// Neighbor information for pointer network
struct NeighborInfo
{
  uint32_t nodeId;
  Ipv4Address address;
  double distance;
  double linkQuality;
  double trustScore;
  double energy;
  std::vector<float> compressedEmbedding;
  AstroAction lastAction;
  MissionRole role;
};

// Action result from the policy
struct PolicyAction
{
  AstroAction action;
  uint32_t selectedNeighborId;     // For Forward action
  Ipv4Address selectedNeighborAddr;
  MissionRole targetRole;          // For RoleSwitch action
  double actionProbability;        // pi(a|o) for logging
};

/**
 * \brief Legacy MLP helper for decentralized route-selection decisions
 *
 * Retained for compatibility with the older executable scaffold. The current
 * A3D-BSM paper does not rely on this helper for suppression calibration.
 */
class MappoAgent : public Object
{
public:
  static TypeId GetTypeId (void);

  MappoAgent ();
  virtual ~MappoAgent ();

  /**
   * Load pre-trained actor weights from file.
   * Format: binary file with weight matrices for each layer.
   */
  bool LoadWeights (const std::string &filename);

  /**
   * Initialize with random weights (for training or random baseline).
   */
  void InitializeRandom (uint32_t seed = 42);

  /**
   * Select an action given the current observation.
   *
   * The observation o_i(t) = [z_i(t) || mean_pool(trusted_neighbor_intents)] is
   * constructed internally from:
   * - embedding: SLM embedding z_i(t) of dim d_z
   * - neighborIntents: aggregated intent from trusted neighbors
   * - neighbors: current neighbor set with metadata
   * - currentRole: current mission role
   *
   * Returns the selected action including neighbor pointer for Forward.
   */
  PolicyAction SelectAction (const std::vector<float> &embedding,
                             const std::vector<float> &aggregatedIntent,
                             const std::vector<NeighborInfo> &neighbors,
                             MissionRole currentRole,
                             TrafficClass headOfLineTrafficClass);

  /**
   * Compute the pointer network score for each neighbor (Eq. 9).
   * j* = argmax_j w^T tanh(W_q z_i + W_k z_hat_j)
   *
   * \param embedding SLM embedding of the current agent
   * \param neighbors Neighbor information including lifted embeddings
   * \returns Scores for each neighbor (higher = preferred)
   */
  std::vector<double> ComputePointerScores (const std::vector<float> &embedding,
                                             const std::vector<NeighborInfo> &neighbors) const;

  /**
   * Compute the multi-objective reward for the last action (Eq. 5).
   * Used for logging and offline training data collection.
   */
  double ComputeReward (bool delivered, double packetAge, double maxDelay,
                        bool aggregated, double channelOccupancy,
                        double energyPerBit, bool suppressedRedundant) const;

  // Reward weights (alpha_1 through alpha_6 from Eq. 5)
  void SetRewardWeights (double a1, double a2, double a3,
                         double a4, double a5, double a6);

  void SetNodeId (uint32_t id) { m_nodeId = id; }
  uint32_t GetNodeId () const { return m_nodeId; }

  // Episode-level reward tracking (Eq. 6)
  void ResetEpisodeReward ();
  void AccumulateReward (double r);
  double GetEpisodeReward () const { return m_episodeReward; }

  // Training data logging
  void EnableLogging (const std::string &logDir);
  void LogTransition (const std::vector<float> &state, AstroAction action,
                      double reward, const std::vector<float> &nextState);

private:
  uint32_t m_nodeId;

  // Actor network weights (2-layer MLP: [256, 128])
  // Layer 1: W1 (256 x inputDim), b1 (256)
  // Layer 2: W2 (128 x 256), b2 (128)
  // Action head: Wa (numActions x 128), ba (numActions)
  std::vector<std::vector<float>> m_W1, m_W2, m_Wa;
  std::vector<float> m_b1, m_b2, m_ba;

  // Pointer network weights (Eq. 9)
  // W_q (d_z x d_z), W_k (d_z x d_z), w (d_z)
  std::vector<std::vector<float>> m_Wq, m_Wk;
  std::vector<float> m_wPointer;

  // Reward weights
  double m_alpha1, m_alpha2, m_alpha3, m_alpha4, m_alpha5, m_alpha6;

  // Episode tracking
  double m_episodeReward;
  double m_gamma;  // Discount factor

  // Logging
  bool m_loggingEnabled;
  std::string m_logDir;

  // Neural network forward pass helpers
  std::vector<float> MatVecMul (const std::vector<std::vector<float>> &W,
                                 const std::vector<float> &x) const;
  std::vector<float> AddBias (const std::vector<float> &x,
                               const std::vector<float> &b) const;
  std::vector<float> ReLU (const std::vector<float> &x) const;
  std::vector<float> Softmax (const std::vector<float> &x) const;
  std::vector<float> Tanh (const std::vector<float> &x) const;

  // Random number generation
  uint32_t m_seed;
  double Uniform01 () const;
};

} // namespace astro
} // namespace ns3

#endif /* MAPPO_AGENT_H */
