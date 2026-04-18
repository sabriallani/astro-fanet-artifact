/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "astro-helper.h"
#include "ns3/astro-routing-protocol.h"
#include "ns3/node.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AstroHelper");

AstroHelper::AstroHelper ()
{
  m_factory.SetTypeId ("ns3::astro::AstroRoutingProtocol");
}

AstroHelper::~AstroHelper ()
{
}

AstroHelper*
AstroHelper::Copy (void) const
{
  return new AstroHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
AstroHelper::Create (Ptr<Node> node) const
{
  Ptr<astro::AstroRoutingProtocol> protocol = m_factory.Create<astro::AstroRoutingProtocol> ();
  node->AggregateObject (protocol);
  return protocol;
}

void
AstroHelper::Set (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

} // namespace ns3
