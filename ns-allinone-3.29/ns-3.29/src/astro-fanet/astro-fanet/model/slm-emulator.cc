/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "slm-emulator.h"
#include "ns3/log.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <numeric>
#include <random>

namespace ns3 {
namespace astro {

NS_LOG_COMPONENT_DEFINE ("SlmEmulator");
NS_OBJECT_ENSURE_REGISTERED (SlmEmulator);

// ========================================================================
// RawStateVector
// ========================================================================
std::vector<double>
RawStateVector::ToFlat () const
{
  return {posX, posY, posZ, velX, velY, velZ, energy,
          static_cast<double>(neighborCount),
          static_cast<double>(queueEmg), static_cast<double>(queueCmd),
          static_cast<double>(queueSen), static_cast<double>(queueTel),
          static_cast<double>(role)};
}

double
RawStateVector::DistanceTo (const RawStateVector &other) const
{
  auto a = ToFlat ();
  auto b = other.ToFlat ();
  double sum = 0.0;
  for (size_t i = 0; i < a.size (); i++)
    {
      double diff = a[i] - b[i];
      sum += diff * diff;
    }
  return std::sqrt (sum);
}

// ========================================================================
// SlmEmulator
// ========================================================================
TypeId
SlmEmulator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::astro::SlmEmulator")
    .SetParent<Object> ()
    .SetGroupName ("AstroFanet")
    .AddConstructor<SlmEmulator> ();
  return tid;
}

SlmEmulator::SlmEmulator ()
  : m_k (5)
{
}

SlmEmulator::~SlmEmulator ()
{
}

bool
SlmEmulator::LoadEmbeddings (const std::string &filename)
{
  std::ifstream file (filename);
  if (!file.is_open ())
    {
      NS_LOG_WARN ("Cannot open embedding file: " << filename);
      return false;
    }

  std::string line;
  // Skip header
  std::getline (file, line);

  while (std::getline (file, line))
    {
      std::istringstream iss (line);
      EmbeddingEntry entry;
      char comma;

      iss >> entry.state.posX >> comma >> entry.state.posY >> comma >> entry.state.posZ >> comma
          >> entry.state.velX >> comma >> entry.state.velY >> comma >> entry.state.velZ >> comma
          >> entry.state.energy >> comma >> entry.state.neighborCount >> comma
          >> entry.state.queueEmg >> comma >> entry.state.queueCmd >> comma
          >> entry.state.queueSen >> comma >> entry.state.queueTel >> comma;

      uint32_t roleVal;
      iss >> roleVal >> comma;
      entry.state.role = static_cast<MissionRole>(roleVal);

      entry.embedding.resize (EMBEDDING_DIM);
      for (uint32_t i = 0; i < EMBEDDING_DIM; i++)
        {
          iss >> entry.embedding[i];
          if (i < EMBEDDING_DIM - 1)
            iss >> comma;
        }

      m_entries.push_back (entry);
    }

  NS_LOG_INFO ("Loaded " << m_entries.size () << " embedding entries");
  ComputeNormalizationParams ();
  return true;
}

void
SlmEmulator::GenerateSyntheticEmbeddings (uint32_t count, uint32_t seed)
{
  NS_LOG_INFO ("Generating " << count << " synthetic embeddings with seed " << seed);

  std::mt19937 rng (seed);
  std::uniform_real_distribution<double> posDist (0.0, 2000.0);   // 2km x 2km
  std::uniform_real_distribution<double> altDist (50.0, 250.0);   // 200m altitude range
  std::uniform_real_distribution<double> velDist (-25.0, 25.0);   // 10-25 m/s
  std::uniform_real_distribution<double> energyDist (0.1, 1.0);
  std::uniform_int_distribution<uint32_t> neighborDist (0, 15);
  std::uniform_int_distribution<uint32_t> queueDist (0, 20);
  std::uniform_int_distribution<uint32_t> roleDist (0, 3);
  std::normal_distribution<float> embDist (0.0f, 1.0f);

  m_entries.clear ();
  m_entries.reserve (count);

  for (uint32_t i = 0; i < count; i++)
    {
      EmbeddingEntry entry;
      entry.state.posX = posDist (rng);
      entry.state.posY = posDist (rng);
      entry.state.posZ = altDist (rng);
      entry.state.velX = velDist (rng);
      entry.state.velY = velDist (rng);
      entry.state.velZ = velDist (rng) * 0.3;  // Less vertical variation
      entry.state.energy = energyDist (rng);
      entry.state.neighborCount = neighborDist (rng);
      entry.state.queueEmg = queueDist (rng) / 5;
      entry.state.queueCmd = queueDist (rng) / 3;
      entry.state.queueSen = queueDist (rng);
      entry.state.queueTel = queueDist (rng) / 2;
      entry.state.role = static_cast<MissionRole>(roleDist (rng));

      // Generate structured synthetic embedding:
      // The embedding encodes spatial, mobility, and resource information
      // in a way that mimics SLM semantic encoding.
      entry.embedding.resize (EMBEDDING_DIM);

      // First 64 dims: spatial encoding (position-dependent)
      for (uint32_t d = 0; d < 64; d++)
        {
          double freq = (d + 1.0) * M_PI / 2000.0;
          if (d % 2 == 0)
            entry.embedding[d] = static_cast<float>(std::sin (entry.state.posX * freq + entry.state.posZ * freq * 0.5));
          else
            entry.embedding[d] = static_cast<float>(std::cos (entry.state.posY * freq + entry.state.posZ * freq * 0.5));
        }

      // Next 64 dims: velocity/mobility encoding
      double speed = std::sqrt (entry.state.velX * entry.state.velX +
                                entry.state.velY * entry.state.velY +
                                entry.state.velZ * entry.state.velZ);
      for (uint32_t d = 64; d < 128; d++)
        {
          double phase = speed * (d - 64) * 0.1;
          entry.embedding[d] = static_cast<float>(std::tanh (phase + entry.state.velX * 0.05));
          entry.embedding[d] += embDist (rng) * 0.1f;
        }

      // Next 64 dims: resource/queue encoding
      for (uint32_t d = 128; d < 192; d++)
        {
          double resourceState = entry.state.energy * 2.0 - 1.0;
          double queuePressure = (entry.state.queueEmg * 4.0 + entry.state.queueCmd * 3.0 +
                                  entry.state.queueSen * 2.0 + entry.state.queueTel) / 100.0;
          entry.embedding[d] = static_cast<float>(std::tanh (resourceState - queuePressure + (d - 128) * 0.05));
          entry.embedding[d] += embDist (rng) * 0.1f;
        }

      // Last 64 dims: topology/context encoding
      for (uint32_t d = 192; d < 256; d++)
        {
          double neighborEffect = entry.state.neighborCount / 15.0;
          double roleEffect = entry.state.role / 3.0;
          entry.embedding[d] = static_cast<float>(std::tanh (neighborEffect + roleEffect * (d - 192) * 0.02));
          entry.embedding[d] += embDist (rng) * 0.15f;
        }

      // L2 normalize the embedding
      float norm = 0.0f;
      for (auto &v : entry.embedding)
        norm += v * v;
      norm = std::sqrt (norm);
      if (norm > 1e-6f)
        {
          for (auto &v : entry.embedding)
            v /= norm;
        }

      m_entries.push_back (entry);
    }

  ComputeNormalizationParams ();
  NS_LOG_INFO ("Generated " << m_entries.size () << " synthetic embeddings");
}

void
SlmEmulator::ComputeNormalizationParams ()
{
  if (m_entries.empty ())
    return;

  uint32_t featDim = m_entries[0].state.ToFlat ().size ();
  m_featureMeans.assign (featDim, 0.0);
  m_featureStds.assign (featDim, 1.0);

  // Compute means
  for (const auto &entry : m_entries)
    {
      auto flat = entry.state.ToFlat ();
      for (uint32_t i = 0; i < featDim; i++)
        m_featureMeans[i] += flat[i];
    }
  for (auto &m : m_featureMeans)
    m /= m_entries.size ();

  // Compute stds
  for (const auto &entry : m_entries)
    {
      auto flat = entry.state.ToFlat ();
      for (uint32_t i = 0; i < featDim; i++)
        {
          double diff = flat[i] - m_featureMeans[i];
          m_featureStds[i] += diff * diff;
        }
    }
  for (auto &s : m_featureStds)
    {
      s = std::sqrt (s / m_entries.size ());
      if (s < 1e-8)
        s = 1.0;
    }
}

std::vector<double>
SlmEmulator::Normalize (const std::vector<double> &raw) const
{
  std::vector<double> normalized (raw.size ());
  for (size_t i = 0; i < raw.size () && i < m_featureMeans.size (); i++)
    normalized[i] = (raw[i] - m_featureMeans[i]) / m_featureStds[i];
  return normalized;
}

std::vector<float>
SlmEmulator::Encode (const RawStateVector &state) const
{
  if (m_entries.empty ())
    {
      NS_LOG_WARN ("No embeddings loaded, returning zero vector");
      return std::vector<float> (EMBEDDING_DIM, 0.0f);
    }

  // Normalize the query state
  auto queryRaw = state.ToFlat ();
  auto queryNorm = Normalize (queryRaw);

  // Find k nearest neighbors
  struct DistIdx {
    double dist;
    uint32_t idx;
    bool operator< (const DistIdx &other) const { return dist < other.dist; }
  };

  std::vector<DistIdx> distances (m_entries.size ());
  for (uint32_t i = 0; i < m_entries.size (); i++)
    {
      auto entryNorm = Normalize (m_entries[i].state.ToFlat ());
      double dist = 0.0;
      for (size_t d = 0; d < queryNorm.size (); d++)
        {
          double diff = queryNorm[d] - entryNorm[d];
          dist += diff * diff;
        }
      distances[i] = {std::sqrt (dist), i};
    }

  // Partial sort to get k nearest
  uint32_t k = std::min (m_k, static_cast<uint32_t>(distances.size ()));
  std::partial_sort (distances.begin (), distances.begin () + k, distances.end ());

  // Inverse-distance-weighted interpolation
  std::vector<float> result (EMBEDDING_DIM, 0.0f);
  double weightSum = 0.0;

  for (uint32_t i = 0; i < k; i++)
    {
      double weight = 1.0 / (distances[i].dist + 1e-8);
      weightSum += weight;

      const auto &emb = m_entries[distances[i].idx].embedding;
      for (uint32_t d = 0; d < EMBEDDING_DIM && d < emb.size (); d++)
        result[d] += static_cast<float>(weight * emb[d]);
    }

  // Normalize by total weight
  if (weightSum > 1e-8)
    {
      for (auto &v : result)
        v /= static_cast<float>(weightSum);
    }

  return result;
}

std::vector<float>
SlmEmulator::Compress (const std::vector<float> &embedding) const
{
  // Simple compression: take first COMPRESSED_EMBED_DIM components
  // In a full implementation, this would use a learned compression matrix
  std::vector<float> compressed (COMPRESSED_EMBED_DIM, 0.0f);
  for (uint32_t i = 0; i < COMPRESSED_EMBED_DIM && i < embedding.size (); i++)
    compressed[i] = embedding[i];
  return compressed;
}

std::vector<float>
SlmEmulator::Lift (const std::vector<float> &compressed) const
{
  // Approximate lift: pad with zeros (W_lift approximation)
  // In full implementation, this uses the learned linear map W_lift
  std::vector<float> lifted (EMBEDDING_DIM, 0.0f);
  for (uint32_t i = 0; i < COMPRESSED_EMBED_DIM && i < compressed.size (); i++)
    lifted[i] = compressed[i];
  return lifted;
}

} // namespace astro
} // namespace ns3
