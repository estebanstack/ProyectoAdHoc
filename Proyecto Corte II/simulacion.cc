#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocNetworkAodvExample");

AnimationInterface* pAnim = nullptr; // Puntero global para NetAnim

void DesactivarNodo (Ptr<Node> node)
{
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
  if (mob)
  {
    mob->SetPosition (Vector (1000.0, 1000.0, 0.0)); // Llevarlo muy lejos
  }
  
  if (pAnim)
  {
    pAnim->UpdateNodeDescription(node, "OFF"); // Cambiar descripción
    pAnim->UpdateNodeColor(node, 255, 0, 0);    // Cambiar color a rojo
  }
  
  std::cout << "Nodo " << node->GetId() << " \"desactivado\" (movido lejos)." << std::endl;
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  NodeContainer nodes;
  nodes.Create (5);
  
  WifiHelper wifi;
  
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  
  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");
  
  NetDeviceContainer devices;
  devices = wifi.Install (phy, mac, nodes);
  
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  positionAlloc->Add (Vector (100.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 50.0, 0.0));
  positionAlloc->Add (Vector (50.0, 50.0, 0.0));
  
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue (positionAlloc));
  mobility.Install (nodes);
  
  AnimationInterface anim ("scratch/adHocAnim.xml");
  pAnim = &anim; // Asignar puntero global
  
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);
  
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  
  uint16_t port = 9;
  
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));
  
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));
  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  
  // Programar la "eliminación" del nodo 3 en el segundo 5
  Simulator::Schedule (Seconds (5.0), &DesactivarNodo, nodes.Get (3));
  
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  
  Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (serverApp.Get (0));
  uint32_t totalPacketsReceived = udpServer->GetReceived ();
  std::cout << "Total de paquetes recibidos por el servidor: " << totalPacketsReceived << std::endl;
  
  double throughput = (totalPacketsReceived * 1024 * 8) / (8.0 * 1e6);
  std::cout << "Throughput promedio: " << throughput << " Mbps" << std::endl;
  
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
    std::cout << "\nFlow ID: " << iter->first << " Source: " << t.sourceAddress << " Destination: " << t.destinationAddress << std::endl;
    std::cout << "\tTx Packets: " << iter->second.txPackets << std::endl;
    std::cout << "\tRx Packets: " << iter->second.rxPackets << std::endl;
    std::cout << "\tLost Packets: " << (iter->second.txPackets - iter->second.rxPackets) << std::endl;
    std::cout << "\tDelay promedio: " << (iter->second.delaySum.GetSeconds () / iter->second.rxPackets) << " s" << std::endl;
    std::cout << "\tThroughput promedio: " << (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds () - iter->second.timeFirstTxPacket.GetSeconds ()) / 1e6) << " Mbps" << std::endl;
  }
  
  Simulator::Destroy ();
  
  return 0;
}