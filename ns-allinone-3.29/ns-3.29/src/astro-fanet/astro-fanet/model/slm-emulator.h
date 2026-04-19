/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ASTRO-FANET legacy context-vector emulator.
 *
 * This class is retained by the executable ns-3 scaffold for compatibility
 * with older route-selection code. It is not the A3D-BSM calibration method
 * used by the current manuscript.
 */
#ifndef SLM_EMULATOR_H
#define SLM_EMULATOR_H

#include "ns3/object.h"
#include "astro-packet.h"
#include <vector>
#include <string>

namespace ns3 {
namespace astro {

// Raw state vector for k-NN lookup (position, velocity, energy, neighbor count, queue lengths)
struct RawStateVector
{
  double posX, posY, posZ;
  double velX, velY, velZ;
  double energy;
  uint32_t neighborCount;
  uint32_t queueEmg, queueCmd, queueSen, queueTel;
  MissionRole role;

  // Convert to flat vector for distance computation
  std::vector<double> ToFlat () const;

  // Euclidean distance in normalized feature space
  double DistanceTo (const RawStateVector &other) const;
};

// Pre-computed embedding entry
struct EmbeddingEntry
{
  RawStateVector state;
  std::vector<float> embedding;  // R^256
};

/**
 * \brief Legacy context-vector emulator using k-NN interpolation.
 *
 * This class supports the older executable scaffold:
 * 1. Load 50,000 pre-computed (state, embedding) pairs from file
 * 2. For a new state, find k=5 nearest neighbors in raw state space
 * 3. Return inverse-distance-weighted interpolation of their embeddings
 */
class SlmEmulator : public Object
{
public:
  static TypeId GetTypeId (void);

  SlmEmulator ();
  virtual ~SlmEmulator ();

  /**
   * Load pre-computed embeddings from CSV file.
   * Format: posX,posY,posZ,velX,velY,velZ,energy,nNeighbors,qEmg,qCmd,qSen,qTel,role,e0,e1,...,e255
   */
  bool LoadEmbeddings (const std::string &filename);

  /**
   * Generate synthetic embeddings for testing (when no pre-computed file exists).
   * Creates representative state-embedding pairs using deterministic pseudo-random mapping.
   */
  void GenerateSyntheticEmbeddings (uint32_t count = 50000, uint32_t seed = 42);

  /**
   * Compute SLM embedding for a given state via k-NN interpolation (Eq. 2).
   * Returns z_i(t) in R^d_z (d_z = 256).
   */
  std::vector<float> Encode (const RawStateVector &state) const;

  /**
   * Compress embedding to d_iota dimensions for beacon piggybacking.
   * Uses simple truncation + scaling (approximation of the learned linear map W_lift).
   */
  std::vector<float> Compress (const std::vector<float> &embedding) const;

  /**
   * Lift compressed embedding back to d_z dimensions (Eq. 9: W_lift * z_tilde).
   */
  std::vector<float> Lift (const std::vector<float> &compressed) const;

  void SetK (uint32_t k) { m_k = k; }
  uint32_t GetK (void) const { return m_k; }

  uint32_t GetEmbeddingDim (void) const { return EMBEDDING_DIM; }
  uint32_t GetNumEntries (void) const { return m_entries.size (); }

private:
  uint32_t m_k;  // Number of nearest neighbors (default: 5)
  std::vector<EmbeddingEntry> m_entries;

  // Normalization parameters for feature space
  std::vector<double> m_featureMeans;
  std::vector<double> m_featureStds;

  void ComputeNormalizationParams ();
  std::vector<double> Normalize (const std::vector<double> &raw) const;
};

} // namespace astro
} // namespace ns3

#endif /* SLM_EMULATOR_H */
