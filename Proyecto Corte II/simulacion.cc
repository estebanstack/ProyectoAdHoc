// Inclusión de los módulos principales de ns-3 necesarios para la simulación
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

// Definición de componente para logs
NS_LOG_COMPONENT_DEFINE ("AdhocNetworkAodvExample");

// Puntero global a la interfaz de animación NetAnim
AnimationInterface* pAnim = nullptr;

// Función para simular la desactivación de un nodo moviéndolo lejos del área de cobertura
void DesactivarNodo (Ptr<Node> node)
{
  // Obtener el modelo de movilidad del nodo
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
  if (mob)
  {
    // Reubicar el nodo a una posición fuera del rango de comunicación
    mob->SetPosition (Vector (1000.0, 1000.0, 0.0));
  }

  // Cambiar visualmente su estado en NetAnim
  if (pAnim)
  {
    pAnim->UpdateNodeDescription(node, "OFF"); // Etiquetar como "OFF"
    pAnim->UpdateNodeColor(node, 255, 0, 0);    // Cambiar color a rojo
  }

  std::cout << "Nodo " << node->GetId() << " \"desactivado\" (movido lejos)." << std::endl;
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Crear 5 nodos en la red
  NodeContainer nodes;
  nodes.Create (5);

  // Configuración del Wi-Fi
  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac"); // Tipo de MAC ad hoc
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes); // Instalar dispositivos

  // Configuración de movilidad
  MobilityHelper mobility;

  // Posiciones iniciales para los nodos
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  positionAlloc->Add (Vector (100.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 50.0, 0.0));
  positionAlloc->Add (Vector (50.0, 50.0, 0.0));

  // Definir el área en la que se moverán aleatoriamente los nodos
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

  // Modelo de movilidad aleatorio
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue (positionAlloc));
  mobility.Install (nodes);

  // Configuración de animación
  AnimationInterface anim ("scratch/adHocAnim.xml");
  pAnim = &anim;

  // Instalación del protocolo de enrutamiento AODV
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  // Asignación de direcciones IP
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Configurar aplicaciones UDP
  uint16_t port = 9;

  // Nodo 1 actúa como servidor UDP
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Nodo 0 envía paquetes al nodo 1 como cliente UDP
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Activar monitor de flujo para estadísticas
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Programar desactivación del nodo 3 a los 5 segundos
  Simulator::Schedule (Seconds (5.0), &DesactivarNodo, nodes.Get (3));

  // Correr simulación
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Obtener estadísticas del servidor
  Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (serverApp.Get (0));
  uint32_t totalPacketsReceived = udpServer->GetReceived ();
  std::cout << "Total de paquetes recibidos por el servidor: " << totalPacketsReceived << std::endl;

  // Calcular throughput
  double throughput = (totalPacketsReceived * 1024 * 8) / (8.0 * 1e6);
  std::cout << "Throughput promedio: " << throughput << " Mbps" << std::endl;

  // Mostrar estadísticas de FlowMonitor
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

  // Finalizar simulación
  Simulator::Destroy ();
  
  return 0;
}
