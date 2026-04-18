/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "mappo-agent.h"
#include "ns3/log.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <fstream>
#include <numeric>

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("MappoAgent");
NS_OBJECT_ENSURE_REGISTERED (MappoAgent);

TypeId
MappoAgent::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::astro::MappoAgent")
    .SetParent<Object> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<MappoAgent> ();
  return tid;
}

MappoAgent::MappoAgent ()
  : m_nodeId (0),
    m_alpha1 (1.0), m_alpha2 (0.5), m_alpha3 (0.3),
    m_alpha4 (0.8), m_alpha5 (0.4), m_alpha6 (0.6),
    m_episodeReward (0.0),
    m_gamma (0.99),
    m_loggingEnabled (false),
    m_seed (42)
{
  InitializeRandom (m_seed);
}

MappoAgent::~MappoAgent ()
{
}

void
MappoAgent::InitializeRandom (uint32_t seed)
{
  m_seed = seed;
  std::mt19937 rng (seed);
  std::normal_distribution<float> dist (0.0f, 0.02f);

  // Input dimension: d_z (256) + d_iota (16) for aggregated intent = 272
  uint32_t inputDim = EMBEDDING_DIM + INTENT_DIM;

  // Layer 1: 256 x inputDim
  m_W1.resize (256, std::vector<float>(inputDim, 0.0f));
  m_b1.resize (256, 0.0f);
  for (auto &row : m_W1)
    for (auto &w : row)
      w = dist (rng);

  // Layer 2: 128 x 256
  m_W2.resize (128, std::vector<float>(256, 0.0f));
  m_b2.resize (128, 0.0f);
  for (auto &row : m_W2)
    for (auto &w : row)
      w = dist (rng);

  // Action head: 5 x 128 (5 base actions: forward, aggregate, suppress, broadcast, roleswitch)
  m_Wa.resize (5, std::vector<float>(128, 0.0f));
  m_ba.resize (5, 0.0f);
  for (auto &row : m_Wa)
    for (auto &w : row)
      w = dist (rng);

  // Pointer network weights (Eq. 9)
  // W_q: d_z x d_z, W_k: d_z x d_z, w: d_z
  m_Wq.resize (EMBEDDING_DIM, std::vector<float>(EMBEDDING_DIM, 0.0f));
  m_Wk.resize (EMBEDDING_DIM, std::vector<float>(EMBEDDING_DIM, 0.0f));
  m_wPointer.resize (EMBEDDING_DIM, 0.0f);

  std::normal_distribution<float> ptrDist (0.0f, 0.01f);
  for (auto &row : m_Wq)
    for (auto &w : row)
      w = ptrDist (rng);
  for (auto &row : m_Wk)
    for (auto &w : row)
      w = ptrDist (rng);
  for (auto &w : m_wPointer)
    w = ptrDist (rng);
}

bool
MappoAgent::LoadWeights (const std::string &filename)
{
  std::ifstream file (filename, std::ios::binary);
  if (!file.is_open ())
    {
      NS_LOG_WARN ("Cannot load weights from: " << filename);
      return false;
    }

  // Read layer dimensions and weights
  auto readMatrix = [&file](std::vector<std::vector<float>> &mat, uint32_t rows, uint32_t cols) {
    mat.resize (rows, std::vector<float>(cols));
    for (uint32_t i = 0; i < rows; i++)
      file.read (reinterpret_cast<char*>(mat[i].data ()), cols * sizeof (float));
  };

  auto readVector = [&file](std::vector<float> &vec, uint32_t dim) {
    vec.resize (dim);
    file.read (reinterpret_cast<char*>(vec.data ()), dim * sizeof (float));
  };

  uint32_t inputDim = EMBEDDING_DIM + INTENT_DIM;
  readMatrix (m_W1, 256, inputDim);
  readVector (m_b1, 256);
  readMatrix (m_W2, 128, 256);
  readVector (m_b2, 128);
  readMatrix (m_Wa, 5, 128);
  readVector (m_ba, 5);
  readMatrix (m_Wq, EMBEDDING_DIM, EMBEDDING_DIM);
  readMatrix (m_Wk, EMBEDDING_DIM, EMBEDDING_DIM);
  readVector (m_wPointer, EMBEDDING_DIM);

  NS_LOG_INFO ("Loaded MAPPO weights from " << filename);
  return true;
}

// ---- Neural network helpers ----

std::vector<float>
MappoAgent::MatVecMul (const std::vector<std::vector<float>> &W,
                        const std::vector<float> &x) const
{
  std::vector<float> result (W.size (), 0.0f);
  for (size_t i = 0; i < W.size (); i++)
    {
      float sum = 0.0f;
      size_t dim = std::min (W[i].size (), x.size ());
      for (size_t j = 0; j < dim; j++)
        sum += W[i][j] * x[j];
      result[i] = sum;
    }
  return result;
}

std::vector<float>
MappoAgent::AddBias (const std::vector<float> &x, const std::vector<float> &b) const
{
  std::vector<float> result (x.size ());
  for (size_t i = 0; i < x.size (); i++)
    result[i] = x[i] + (i < b.size () ? b[i] : 0.0f);
  return result;
}

std::vector<float>
MappoAgent::ReLU (const std::vector<float> &x) const
{
  std::vector<float> result (x.size ());
  for (size_t i = 0; i < x.size (); i++)
    result[i] = std::max (0.0f, x[i]);
  return result;
}

std::vector<float>
MappoAgent::Softmax (const std::vector<float> &x) const
{
  std::vector<float> result (x.size ());
  float maxVal = *std::max_element (x.begin (), x.end ());
  float sum = 0.0f;
  for (size_t i = 0; i < x.size (); i++)
    {
      result[i] = std::exp (x[i] - maxVal);
      sum += result[i];
    }
  for (auto &v : result)
    v /= (sum + 1e-8f);
  return result;
}

std::vector<float>
MappoAgent::Tanh (const std::vector<float> &x) const
{
  std::vector<float> result (x.size ());
  for (size_t i = 0; i < x.size (); i++)
    result[i] = std::tanh (x[i]);
  return result;
}

double
MappoAgent::Uniform01 () const
{
  // Simple LCG for reproducible randomness in C++ (const-correct)
  static thread_local std::mt19937 gen (m_seed);
  static thread_local std::uniform_real_distribution<double> dis (0.0, 1.0);
  return dis (gen);
}

// ---- Core policy methods ----

PolicyAction
MappoAgent::SelectAction (const std::vector<float> &embedding,
                           const std::vector<float> &aggregatedIntent,
                           const std::vector<NeighborInfo> &neighbors,
                           MissionRole currentRole,
                           TrafficClass headOfLineTrafficClass)
{
  PolicyAction result;
  result.action = ACTION_FORWARD;
  result.selectedNeighborId = 0;
  result.selectedNeighborAddr = Ipv4Address::GetBroadcast ();
  result.targetRole = currentRole;
  result.actionProbability = 0.0;

  // Construct observation o_i(t) = [z_i(t) || mean(iota_trusted_neighbors)]
  std::vector<float> observation;
  observation.reserve (EMBEDDING_DIM + INTENT_DIM);

  // Add embedding
  for (uint32_t i = 0; i < EMBEDDING_DIM; i++)
    observation.push_back (i < embedding.size () ? embedding[i] : 0.0f);

  // Add aggregated intent
  for (uint32_t i = 0; i < INTENT_DIM; i++)
    observation.push_back (i < aggregatedIntent.size () ? aggregatedIntent[i] : 0.0f);

  // Forward pass through actor network
  // Layer 1: h1 = ReLU(W1 * o + b1)
  auto h1 = ReLU (AddBias (MatVecMul (m_W1, observation), m_b1));

  // Layer 2: h2 = ReLU(W2 * h1 + b2)
  auto h2 = ReLU (AddBias (MatVecMul (m_W2, h1), m_b2));

  // Action logits: logits = Wa * h2 + ba
  auto logits = AddBias (MatVecMul (m_Wa, h2), m_ba);

  // Mask infeasible actions
  // If no neighbors, Forward is infeasible
  if (neighbors.empty ())
    logits[ACTION_FORWARD] = -1e9f;

  // Apply softmax to get action probabilities
  auto probs = Softmax (logits);

  // Sample action from categorical distribution
  double r = Uniform01 ();
  double cumSum = 0.0;
  uint8_t selectedAction = ACTION_BROADCAST;  // Default fallback

  for (size_t i = 0; i < probs.size (); i++)
    {
      cumSum += probs[i];
      if (r <= cumSum)
        {
          selectedAction = static_cast<uint8_t>(i);
          result.actionProbability = probs[i];
          break;
        }
    }

  result.action = static_cast<AstroAction>(selectedAction);

  // If Forward action selected, use pointer network to select neighbor
  if (result.action == ACTION_FORWARD && !neighbors.empty ())
    {
      auto scores = ComputePointerScores (embedding, neighbors);

      // Select highest-scoring neighbor
      auto maxIt = std::max_element (scores.begin (), scores.end ());
      uint32_t bestIdx = std::distance (scores.begin (), maxIt);

      result.selectedNeighborId = neighbors[bestIdx].nodeId;
      result.selectedNeighborAddr = neighbors[bestIdx].address;
    }

  // If RoleSwitch, select target role (simple heuristic: cycle through roles)
  if (result.action == ACTION_ROLE_SWITCH)
    {
      // Choose role based on current state
      if (neighbors.size () > 5)
        result.targetRole = ROLE_AGGREGATING;
      else if (neighbors.size () <= 2)
        result.targetRole = ROLE_RELAYING;
      else
        result.targetRole = ROLE_GATEWAY;
    }

  return result;
}

std::vector<double>
MappoAgent::ComputePointerScores (const std::vector<float> &embedding,
                                    const std::vector<NeighborInfo> &neighbors) const
{
  // Eq. 9: j* = argmax_j w^T tanh(W_q z_i + W_k z_hat_j)

  // W_q * z_i (computed once)
  auto queryProj = MatVecMul (m_Wq, embedding);

  std::vector<double> scores (neighbors.size (), 0.0);

  for (size_t j = 0; j < neighbors.size (); j++)
    {
      // Lift compressed embedding to full dimension
      std::vector<float> liftedEmb (EMBEDDING_DIM, 0.0f);
      for (size_t d = 0; d < COMPRESSED_EMBED_DIM && d < neighbors[j].compressedEmbedding.size (); d++)
        liftedEmb[d] = neighbors[j].compressedEmbedding[d];

      // W_k * z_hat_j
      auto keyProj = MatVecMul (m_Wk, liftedEmb);

      // tanh(W_q z_i + W_k z_hat_j)
      std::vector<float> combined (EMBEDDING_DIM);
      for (uint32_t d = 0; d < EMBEDDING_DIM; d++)
        combined[d] = std::tanh (queryProj[d] + keyProj[d]);

      // w^T * tanh(...)
      double score = 0.0;
      for (uint32_t d = 0; d < EMBEDDING_DIM; d++)
        score += m_wPointer[d] * combined[d];

      // Augment with distance and link quality heuristics for better initial behavior
      score += (1.0 - neighbors[j].distance / 400.0) * 0.5;  // Closer is better
      score += neighbors[j].linkQuality * 0.3;
      score += neighbors[j].energy * 0.2;

      scores[j] = score;
    }

  return scores;
}

double
MappoAgent::ComputeReward (bool delivered, double packetAge, double maxDelay,
                            bool aggregated, double channelOccupancy,
                            double energyPerBit, bool suppressedRedundant) const
{
  // Eq. 5: r_i(t) = alpha1*R_del + alpha2*F_AoI + alpha3*G_agg
  //                 - alpha4*O_bcast - alpha5*E_bit + alpha6*B_storm

  double R_del = delivered ? 1.0 : 0.0;
  double F_AoI = std::max (0.0, 1.0 - packetAge / maxDelay);
  double G_agg = aggregated ? 1.0 : 0.0;
  double O_bcast = channelOccupancy;
  double E_bit = energyPerBit / 50.0;  // Normalize
  double B_storm = suppressedRedundant ? 1.0 : 0.0;

  return m_alpha1 * R_del + m_alpha2 * F_AoI + m_alpha3 * G_agg
         - m_alpha4 * O_bcast - m_alpha5 * E_bit + m_alpha6 * B_storm;
}

void
MappoAgent::SetRewardWeights (double a1, double a2, double a3,
                               double a4, double a5, double a6)
{
  m_alpha1 = a1; m_alpha2 = a2; m_alpha3 = a3;
  m_alpha4 = a4; m_alpha5 = a5; m_alpha6 = a6;
}

void
MappoAgent::ResetEpisodeReward ()
{
  m_episodeReward = 0.0;
}

void
MappoAgent::AccumulateReward (double r)
{
  m_episodeReward += r;
}

void
MappoAgent::EnableLogging (const std::string &logDir)
{
  m_loggingEnabled = true;
  m_logDir = logDir;
}

void
MappoAgent::LogTransition (const std::vector<float> &state, AstroAction action,
                            double reward, const std::vector<float> &nextState)
{
  if (!m_loggingEnabled)
    return;

  std::string filename = m_logDir + "/transitions_node" + std::to_string (m_nodeId) + ".csv";
  std::ofstream file (filename, std::ios::app);
  if (!file.is_open ())
    return;

  // Write state, action, reward, next_state
  for (size_t i = 0; i < state.size (); i++)
    file << state[i] << (i < state.size () - 1 ? "," : "");
  file << "," << static_cast<int>(action) << "," << reward << ",";
  for (size_t i = 0; i < nextState.size (); i++)
    file << nextState[i] << (i < nextState.size () - 1 ? "," : "");
  file << "\n";
}

} // namespace astro
} // namespace ns3
