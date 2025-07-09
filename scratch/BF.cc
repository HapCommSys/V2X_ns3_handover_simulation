#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include "ns3/epc-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-helper.h"
#include <ns3/lte-ue-net-device.h>
#include <ns3/buildings-helper.h>
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/log.h"

using namespace ns3;
using namespace mmwave;

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  LogComponentEnableAll (LOG_PREFIX_ALL);
  LogComponentEnable ("MmWaveHelper", LOG_LEVEL_DEBUG);
  LogComponentEnable ("MmWaveEnbNetDevice", LOG_LEVEL_DEBUG);
  LogComponentEnable ("MmWaveFlexTtiMacScheduler", LOG_LEVEL_DEBUG);
  // LogComponentEnable ("MmWaveBeamformingModel", LOG_LEVEL_ALL);

  /* Information regarding the traces generated:
   *
   * 1. UE_1_SINR.txt : Gives the SINR for each sub-band
   *    Subframe no.  | Slot No. | Sub-band  | SINR (db)
   *
   * 2. UE_1_Tb_size.txt : Allocated transport block size
   *    Time (micro-sec)  |  Tb-size in bytes
   * */

  Ptr<MmWaveHelper> ptr_mmWave = CreateObject<MmWaveHelper> ();
  /* A configuration example.
   * ptr_mmWave->GetCcPhyParams ().at (0).GetConfigurationParameters ()->SetAttribute("SymbolPerSlot", UintegerValue(30)); */

  double simTime = 1;
  NodeContainer enbNodes;
  NodeContainer ueNodes;
  enbNodes.Create (1);
  ueNodes.Create (2);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper enbmobility;
  enbmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbmobility.SetPositionAllocator (enbPositionAlloc);
  enbmobility.Install (enbNodes);
  BuildingsHelper::Install (enbNodes);

  MobilityHelper uemobility;
  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  uePositionAlloc->Add (Vector (10.0, 0.0, 0.0));
  uemobility.SetPositionAllocator (uePositionAlloc);
  uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  uemobility.Install(ueNodes.Get(0));


  Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable> ();
  speed->SetAttribute ("Min", DoubleValue (20.0));
  speed->SetAttribute ("Max", DoubleValue (40.0));
  uemobility.SetMobilityModel ("ns3::RandomWalk2dOutdoorMobilityModel", "Speed",
                               PointerValue (speed), "Bounds",
                               RectangleValue (Rectangle (-2000.0, 2000.0, -2000.0, 2000.0)));
  uemobility.Install (ueNodes.Get(1));
  Ptr<MobilityModel> mob = ueNodes.Get(1)->GetObject<MobilityModel>();
  mob->SetPosition (Vector (0.0, 10.0, 0.0));

  ptr_mmWave->SetAttribute ("BeamformingModel", StringValue ("ns3::MmWaveSvdBeamforming"));
  // ptr_mmWave->SetAttribute ("E2ModeNr", BooleanValue (true));

  // NetDeviceContainer enbNetDev = ptr_mmWave->InstallEnbDevice (enbNodes);
  // NetDeviceContainer ueNetDev = ptr_mmWave->InstallMcUeDevice (ueNodes);

  // #####################LTE########################
  NodeContainer lteNode;
  lteNode.Create(1);
  Ptr<ListPositionAllocator> lteenbPositionAlloc = CreateObject<ListPositionAllocator> ();
  lteenbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper ltemobility;
  ltemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  ltemobility.SetPositionAllocator (lteenbPositionAlloc);
  ltemobility.Install (lteNode);

  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  ptr_mmWave->SetEpcHelper (epcHelper);
  // Get SGW/PGW and create a single RemoteHost
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);
  // Create the Internet by connecting remoteHost to pgw. Setup routing too
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
      ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NetDeviceContainer lteNetDev = ptr_mmWave->InstallLteEnbDevice (lteNode);
  NetDeviceContainer enbNetDev = ptr_mmWave->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueNetDev = ptr_mmWave->InstallMcUeDevice (ueNodes);
  ptr_mmWave->AddX2Interface (lteNode, enbNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueNetDev));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }
  ptr_mmWave->AttachToClosestEnb (ueNetDev, enbNetDev, lteNetDev);

  uint16_t portUdp = 60000;
  Address sinkLocalAddressUdp (InetSocketAddress (Ipv4Address::GetAny (), portUdp));
  PacketSinkHelper sinkHelperUdp ("ns3::UdpSocketFactory", sinkLocalAddressUdp);
  AddressValue serverAddressUdp (InetSocketAddress (remoteHostAddr, portUdp));

  ApplicationContainer sinkApp;
  sinkApp.Add (sinkHelperUdp.Install (remoteHost));

  ApplicationContainer clientApp;

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      // Full traffic
      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory",
                                           InetSocketAddress (Ipv4Address::GetAny (), 1234));
      sinkApp.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));
      UdpClientHelper dlClient (ueIpIface.GetAddress (u), 1234);
      dlClient.SetAttribute ("Interval", TimeValue (MicroSeconds (500)));
      dlClient.SetAttribute ("MaxPackets", UintegerValue (UINT32_MAX));
      dlClient.SetAttribute ("PacketSize", UintegerValue (1280));
      clientApp.Add (dlClient.Install (remoteHost));
    }
  sinkApp.Start (Seconds (0));
  clientApp.Start (MilliSeconds (100));
  clientApp.Stop (Seconds (simTime - 0.1));
  // ################################################

  // ptr_mmWave->AttachToClosestEnb (ueNetDev, enbNetDev);
  ptr_mmWave->EnableTraces ();

  // // Activate a data radio bearer
  // enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  // EpsBearer bearer (q);
  // ptr_mmWave->ActivateDataRadioBearer (ueNetDev, bearer);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}