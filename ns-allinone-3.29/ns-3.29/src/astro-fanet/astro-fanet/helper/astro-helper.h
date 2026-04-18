/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ASTRO_HELPER_H
#define ASTRO_HELPER_H

#include "ns3/ipv4-routing-helper.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"

namespace ns3 {

/**
 * \brief Helper to install ASTRO-FANET routing protocol on nodes
 */
class AstroHelper : public Ipv4RoutingHelper
{
public:
  AstroHelper ();
  virtual ~AstroHelper ();

  AstroHelper* Copy (void) const;
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

  void Set (std::string name, const AttributeValue &value);

private:
  ObjectFactory m_factory;
};

} // namespace ns3

#endif /* ASTRO_HELPER_H */
