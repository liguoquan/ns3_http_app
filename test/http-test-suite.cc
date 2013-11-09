/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Yufei Cheng
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Yufei Cheng   <yfcheng@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */

#include "ns3/test.h"
#include "ns3/pointer.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/mesh-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simulator.h"
#include "ns3/random-variable.h"
#include "ns3/mobility-helper.h"
#include "ns3/v4ping-helper.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/pcap-file.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"

#include "ns3/http-helper.h"
#include "ns3/http-random-variable.h"

using namespace ns3;

class http::HTTPRandomVariableBase;
/**
 * Test that all the tcp segments generated by an HttpClient application are
 * correctly received by an HttpServer application
 */
class HttpTestCase : public TestCase
{
public:
  HttpTestCase ();
  virtual ~HttpTestCase ();

private:
  virtual void DoRun (void);
};

HttpTestCase::HttpTestCase ()
  : TestCase ("Test that all the tcp packets generated by an httpClient application are correctly received by an httpServer application")
{
}

HttpTestCase::~HttpTestCase ()
{
}

void HttpTestCase::DoRun (void)
{
  NodeContainer n;
  n.Create (2);

  InternetStackHelper internet;
  internet.Install (n);
  //
  // Explicitly create the channels required by the topology (shown above).
  //
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d = pointToPoint.Install (n);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (d);

  uint16_t port = 4000;
  Ptr<http::RuntimeVariable> runtimeVariable = CreateObject<http::RuntimeVariable> ();

  HttpServerHelper httpServer;
  httpServer.SetAttribute ("Local", AddressValue (InetSocketAddress (Ipv4Address::GetAny (), port)));
  httpServer.SetAttribute ("RuntimeVariable", PointerValue (runtimeVariable));
  ApplicationContainer serverApps = httpServer.Install (n.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  HttpClientHelper httpClient;
  httpClient.SetAttribute ("Remote", AddressValue (InetSocketAddress (i.GetAddress (1), port)));
  httpClient.SetAttribute ("RunNo", UintegerValue (1));
  httpClient.SetAttribute ("RuntimeVariable", PointerValue (runtimeVariable));
  httpClient.SetAttribute ("MaxSessions", UintegerValue (1));
  /*
   * No need to define if using automatic traffic generation mode
   */
  httpClient.SetAttribute ("UserPages", UintegerValue (1));
  httpClient.SetAttribute ("UserObjects", UintegerValue (1));
  httpClient.SetAttribute ("UserServerDelay", DoubleValue (0.1));
  httpClient.SetAttribute ("UserRequestGap", DoubleValue (0.1));
  httpClient.SetAttribute ("UserRequestSize", UintegerValue (100));
  httpClient.SetAttribute ("UserResponseSize", UintegerValue (100));
  /*
   * This controls the mode, when using http 1.1 and set the ForcePersistent as false, it will
   * determine the connection type by itself
   */
  httpClient.SetAttribute ("Http1_1", BooleanValue (false));
  httpClient.SetAttribute ("Automatic", BooleanValue (false));
  ApplicationContainer clientApps = httpClient.Install (n.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  NS_TEST_ASSERT_MSG_EQ (httpClient.GetClient ()->GetReceived (), 1, "Did not receive expected number of packets !");
}

// -----------------------------------------------------------------------------
// / Unit test for Http sequence Header
class HttpHeaderTest : public TestCase
{
public:
  HttpHeaderTest ();
  virtual ~HttpHeaderTest ();

private:
  virtual void DoRun (void);
};
HttpHeaderTest::HttpHeaderTest ()
  : TestCase ("Http sequence Header")
{
}
HttpHeaderTest::~HttpHeaderTest ()
{
}
void
HttpHeaderTest::DoRun ()
{
  http::HttpSeqHeader header;
  NS_TEST_EXPECT_MSG_EQ (header.GetSerializedSize () % 4, 0, "length of routing header is not a multiple of 4");

  header.SetLength (500);
  NS_TEST_EXPECT_MSG_EQ (header.GetLength (), 500, "trivial");
  header.SetObject (4);
  NS_TEST_EXPECT_MSG_EQ (header.GetObject (), 4, "trivial");
  header.SetPage (1);
  NS_TEST_EXPECT_MSG_EQ (header.GetPage (), 1, "trivial");

  Ptr<Packet> p = Create<Packet> ();
  p->AddHeader (header);
  uint32_t bytes = p->RemoveHeader (header);
  NS_TEST_EXPECT_MSG_EQ (bytes, 8, "Http header is 8 bytes long");
}

// -----------------------------------------------------------------------------
// / Unit test for Http random variable entries
class HttpRandomVariableTest : public TestCase
{
public:
  HttpRandomVariableTest ();
  virtual ~HttpRandomVariableTest ();

private:
  virtual void DoRun (void);
};
HttpRandomVariableTest::HttpRandomVariableTest ()
  : TestCase ("Http random variable entries")
{
}
HttpRandomVariableTest::~HttpRandomVariableTest ()
{
}
void
HttpRandomVariableTest::DoRun ()
{
  // / Test case for flow arrival rate random variable
  http::HTTPFlowArriveRandomVariableImpl flow (1);
  double avg = flow.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 4.3018, "trivial");
  avg = flow.Average (0);
  NS_TEST_EXPECT_MSG_EQ (avg, 0.0262, "trivial");
  avg = flow.Average (1);
  NS_TEST_EXPECT_MSG_EQ (avg, 3.32, "trivial");

  // / Test case for persistent response file size random variable
  http::HTTPPersistRspSizeRandomVariableImpl perRsp;
  avg = perRsp.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");

  // / Test case for persistent random variable
  http::HTTPPersistentRandomVariableImpl persist;
  avg = persist.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");

  // / Test case for number of pages random variable
  http::HTTPNumPagesRandomVariableImpl numPages;
  avg = numPages.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");

  // / Test case for single object random variable
  http::HTTPSingleObjRandomVariableImpl singleObj;
  avg = singleObj.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");

  // / Test case for objects per page random variable
  http::HTTPObjsPerPageRandomVariableImpl objPerPage;
  avg = objPerPage.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");

  // / Test case for time between pages random variable
  http::HTTPTimeBtwnPagesRandomVariableImpl timeBtwnPages;
  avg = timeBtwnPages.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");

  // / Test case for time between objects random variable
  http::HTTPTimeBtwnObjsRandomVariableImpl timeBtwnObjs;
  avg = timeBtwnObjs.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");

  // / Test case for server delay random variable
  http::HTTPServerDelayRandomVariableImpl serverDelay;
  avg = serverDelay.Average ();
  NS_TEST_EXPECT_MSG_EQ (avg, 0, "trivial");
}

class HttpTestSuite : public TestSuite
{
public:
  HttpTestSuite ();
};

HttpTestSuite::HttpTestSuite ()
  : TestSuite ("http", UNIT)
{
  AddTestCase (new HttpTestCase);
  AddTestCase (new HttpHeaderTest);
  AddTestCase (new HttpRandomVariableTest);
}

static HttpTestSuite HttpTestSuite;