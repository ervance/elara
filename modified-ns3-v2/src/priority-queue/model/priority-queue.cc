/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */


#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "priority-queue.h"

#include "ns3/boolean.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

#include "ns3/ppp-header.h"
#include "ns3/udp-header.h"
#include "ns3/ipv4-header.h"

NS_LOG_COMPONENT_DEFINE ("PriorityQueue");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (PriorityQueue);

TypeId 
PriorityQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PriorityQueue")
    .SetParent<Queue> ()
    .AddConstructor<PriorityQueue> ()
    .AddAttribute ("Mode",
                   "Whether to use bytes (see MaxBytes) or packets (see MaxPackets) as the maximum queue size metric.",
                   EnumValue (QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&PriorityQueue::SetMode),
                   MakeEnumChecker (QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets accepted by each PriorityQueue.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&PriorityQueue::m_maxPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxBytes",
                   "The maximum number of bytes accepted by each PriorityQueue.",
                   UintegerValue (100 * 65535),
                   MakeUintegerAccessor (&PriorityQueue::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
 ;

  return tid;
}

PriorityQueue::PriorityQueue () :
  Queue (),
  m_highPackets (),
  m_lowPackets (),
  m_bytesInHighQueue (0),
  m_bytesInLowQueue (0)
{
  NS_LOG_FUNCTION_NOARGS ();
}

PriorityQueue::~PriorityQueue ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void
PriorityQueue::SetMode (PriorityQueue::QueueMode mode)
{
  NS_LOG_FUNCTION (mode);
  m_mode = mode;
}

PriorityQueue::QueueMode
PriorityQueue::GetMode (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_mode;
}

uint16_t
PriorityQueue::Classify (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  PppHeader ppp;
  p->RemoveHeader (ppp);
  Ipv4Header ip;
  p->RemoveHeader (ip);

  uint16_t priority;

  uint32_t protocol = ip.GetProtocol();
  if (protocol == 17)
    {
      UdpHeader udp;
      p->RemoveHeader (udp);

      if (udp.GetDestinationPort() == 3000)
        {
          NS_LOG_INFO ("\tclassifier: high priority udp");
          //NS_LOG_UNCOND ("\tclassifier: high priority udp " << p->GetUid());
          priority = 1;
        }
      else if (udp.GetDestinationPort() == 2000)
        {
          NS_LOG_INFO ("\tclassifier: low priority udp");
          priority = 0;
        }
      else
        {
          NS_LOG_INFO ("\tclassifier: unrecognized udp");
          NS_LOG_INFO ("\tclassifier: port=" << udp.GetDestinationPort());
          priority = 0;
        }
      p->AddHeader (udp);
    }
  else if (protocol == 6)
    {
      NS_LOG_INFO ("\tclassifier: tcp");
      priority = 2;
    }
  else
    {
      NS_LOG_INFO ("\tclassifier: unrecognized protocol");
      priority = 0;
    }

  p->AddHeader (ip);
  p->AddHeader (ppp);

  return priority;
}

bool
PriorityQueue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  uint16_t priority = Classify (p);
  if (priority == 1)
    {
      if (m_mode == QUEUE_MODE_PACKETS && (m_highPackets.size () >= m_maxPackets))
        {
          NS_LOG_LOGIC ("Queue full (at max packets) -- droppping pkt");
          Drop (p);
          return false;
        }
      
      if (m_mode == QUEUE_MODE_BYTES && (m_bytesInHighQueue + p->GetSize () >= m_maxBytes))
        {
          NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- droppping pkt");
          Drop (p);
          return false;
        }
      
      m_bytesInHighQueue += p->GetSize ();
      m_highPackets.push (p);
      
      NS_LOG_LOGIC ("Number packets " << m_highPackets.size ());
      NS_LOG_LOGIC ("Number bytes " << m_bytesInHighQueue);
      
      return true; 
    }

  else if (priority == 0)
    {
      if (m_mode == QUEUE_MODE_PACKETS && (m_lowPackets.size () >= m_maxPackets))
        {
          NS_LOG_LOGIC ("Queue full (at max packets) -- droppping pkt");
          Drop (p);
          return false;
        }
      
      if (m_mode == QUEUE_MODE_BYTES && (m_bytesInLowQueue + p->GetSize () >= m_maxBytes))
        {
          NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- droppping pkt");
          Drop (p);
          return false;
        }
      
      m_bytesInLowQueue += p->GetSize ();
      m_lowPackets.push (p);
      
      NS_LOG_LOGIC ("Number packets " << m_lowPackets.size ());
      NS_LOG_LOGIC ("Number bytes " << m_bytesInLowQueue);
      
      return true;      
    }

  else 
    {
      return false;
    }
}

Ptr<Packet>
PriorityQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);
  
  if (!m_highPackets.empty ())
    {
      Ptr<Packet> p = m_highPackets.front ();
      m_highPackets.pop ();
      m_bytesInHighQueue -= p->GetSize ();
      
      NS_LOG_LOGIC ("Popped " << p);
      
      NS_LOG_LOGIC ("Number packets " << m_highPackets.size ());
      NS_LOG_LOGIC ("Number bytes " << m_bytesInHighQueue);
      
      return p;
    }
  else
    {
      NS_LOG_LOGIC ("High priority queue empty");

      if (!m_lowPackets.empty ())
        {
          Ptr<Packet> p = m_lowPackets.front ();
          m_lowPackets.pop ();
          m_bytesInLowQueue -= p->GetSize ();
          
          NS_LOG_LOGIC ("Popped " << p);
          
          NS_LOG_LOGIC ("Number packets " << m_lowPackets.size ());
          NS_LOG_LOGIC ("Number bytes " << m_bytesInLowQueue);
          
          return p;
        }
      else
        {
          NS_LOG_LOGIC ("Low priority queue empty");
          return 0;
        } 
    } 
}
  
Ptr<const Packet>
PriorityQueue::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);
  
  if (!m_highPackets.empty ())
    {
      Ptr<Packet> p = m_highPackets.front ();
      
      NS_LOG_LOGIC ("Number packets " << m_highPackets.size ());
      NS_LOG_LOGIC ("Number bytes " << m_bytesInHighQueue);
      
      return p;
    }
  else
    {
      NS_LOG_LOGIC ("High priority queue empty");

      if (!m_lowPackets.empty ())
        {
          Ptr<Packet> p = m_lowPackets.front ();
          
          NS_LOG_LOGIC ("Number packets " << m_lowPackets.size ());
          NS_LOG_LOGIC ("Number bytes " << m_bytesInLowQueue);
          
          return p;
        }
      else
        {
          NS_LOG_LOGIC ("Low priority queue empty");
          return 0;
        }
    }
}

} // namespace ns3