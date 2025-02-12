#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include <vector>
#include <ctime>
#include <functional>  // For std::hash
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IoMTBlockchainNetwork");

class Block {
public:
    std::string previousHash;
    std::string timestamp;
    std::string data;
    std::string hash;
    std::string digitalSignature;

    Block(std::string prevHash, std::string data) {
        previousHash = prevHash;
        timestamp = std::to_string(time(0));  // Using current time as timestamp
        this->data = encryptData(data);  // Encrypt the block data
        hash = generateHash();  // Generate the block hash
        digitalSignature = generateDigitalSignature(hash);  // Sign the block hash
    }

    std::string generateHash() {
        std::string toHash = previousHash + timestamp + data;
        std::hash<std::string> hasher;
        size_t hashValue = hasher(toHash);
        return std::to_string(hashValue);
    }

    std::string encryptData(std::string data) {
        // Placeholder encryption function (implement AES or other encryption here)
        return "Encrypted(" + data + ")";
    }

    std::string generateDigitalSignature(std::string hash) {
        // Placeholder digital signature generation (implement RSA signature here)
        return "Signed(" + hash + ")";
    }

    bool verifyDigitalSignature(std::string signature) {
        // Placeholder verification function (implement RSA signature verification here)
        return signature == digitalSignature;
    }
};

class Blockchain {
public:
    std::vector<Block> chain;

    Blockchain() {
        chain.push_back(Block("0", "Genesis Block"));
    }

    void addBlock(std::string data) {
        std::string prevHash = chain.back().hash;
        chain.push_back(Block(prevHash, data));
    }

    void printChain() {
        for (size_t i = 0; i < chain.size(); i++) {
            NS_LOG_UNCOND("Block " << i);
            NS_LOG_UNCOND("Previous Hash: " << chain[i].previousHash);
            NS_LOG_UNCOND("Timestamp: " << chain[i].timestamp);
            NS_LOG_UNCOND("Data: " << chain[i].data);
            NS_LOG_UNCOND("Hash: " << chain[i].hash);
            NS_LOG_UNCOND("Digital Signature: " << chain[i].digitalSignature);
        }
    }
};

class SecureCommunication {
public:
    // Placeholder method for secure data transmission between nodes
    void sendSecureData(NodeContainer& nodes, std::string data) {
        // Implement SSL/TLS or another secure transmission protocol here
        NS_LOG_UNCOND("Sending secure data: " << data);
    }

    void receiveSecureData(std::string encryptedData) {
        // Decrypt and verify the data here
        NS_LOG_UNCOND("Receiving secure data: " << encryptedData);
    }
};

// Function to simulate a MITM blackhole attack (drops Baxter Infusion Pump traffic)

bool MitmInterceptAndDrop(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &from) {
    // Check if packet belongs to the Baxter Pump (e.g., by port number)
    if (protocol == 17 && from == InetSocketAddress("192.168.1.2", 8080)) { // UDP packet from Baxter
        NS_LOG_UNCOND("MITM Attack: Intercepted Baxter packet!");
        return false; // Drop the packet
    }
    return true; // Allow other packets
}


void SetupMitmAttack(NodeContainer &attackerNode, NodeContainer &wifiNodes, Ipv4InterfaceContainer &wifiInterfaces) {
    Ptr<Node> attacker = attackerNode.Get(0);
    Ptr<Ipv4> ipv4 = attacker->GetObject<Ipv4>();
    
    // Retrieve the routing protocol and cast to static routing
    Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4->GetRoutingProtocol();
    Ptr<Ipv4StaticRouting> attackerRouting = DynamicCast<Ipv4StaticRouting>(routingProtocol);

    // Ensure routing is set up properly
    if (attackerRouting) {
        Ipv4Address targetAddress = wifiInterfaces.GetAddress(0);
        attackerRouting->SetDefaultRoute(targetAddress, 1); // Example configuration
    }

    // Set the receive callback to drop packets
    Ptr<NetDevice> attackerDevice = attacker->GetDevice(0);
    attackerDevice->SetReceiveCallback(MakeCallback(&MitmInterceptAndDrop));
}

int main(int argc, char *argv[])
{
    // Enable logging for specific applications and monitoring
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);

    // Define simulation time
    double simulationTime = 40.0;

    // Define number of nodes for Wi-Fi network (9 in total)
    uint32_t numNodes = 9;

    // Create the container for Wi-Fi nodes (devices)
    NodeContainer wifiNodes;
    wifiNodes.Create(numNodes);

    // Create containers for the Access Point (AP), Hexoskin Shirt, and Attacker Node
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer hexoskinNodes;
    hexoskinNodes.Create(1);
    NodeContainer attackerNode;
    attackerNode.Create(1);

    // Set up the Wi-Fi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = ns3::YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    // Configure the Wi-Fi standard (802.11n) and the Wi-Fi station manager
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    // Set up the MAC layer (for both Wi-Fi stations and AP)
    WifiMacHelper mac;
    Ssid ssid = Ssid("HealthNet_24G");

    // Install Wi-Fi stations (clients)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer wifiDevices = wifi.Install(phy, mac, wifiNodes);

    // Install Wi-Fi Access Point (AP) device
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install internet stack (IP, routing) on all nodes
    InternetStackHelper stack;
    stack.Install(wifiNodes);
    stack.Install(wifiApNode);
    stack.Install(hexoskinNodes);
    stack.Install(attackerNode);

    // Set up IP addressing for the Wi-Fi network
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Define mobility models for all nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes); // Apply mobility to Wi-Fi nodes

    // Set position for the Access Point (AP)
    Ptr<ListPositionAllocator> apPosition = CreateObject<ListPositionAllocator>();
    apPosition->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPosition);
    mobility.Install(wifiApNode);

    // Set position for the Hexoskin Shirt device
    Ptr<ListPositionAllocator> hexoskinPosition = CreateObject<ListPositionAllocator>();
    hexoskinPosition->Add(Vector(20.0, 20.0, 0.0));
    mobility.SetPositionAllocator(hexoskinPosition);
    mobility.Install(hexoskinNodes);

    // Set position for the Attacker Node
    Ptr<ListPositionAllocator> attackerPosition = CreateObject<ListPositionAllocator>();
    attackerPosition->Add(Vector(5.0, 5.0, 0.0));
    mobility.SetPositionAllocator(attackerPosition);
    mobility.Install(attackerNode);

    // Point-to-point link to represent Hexoskin Shirt (using p2p connection)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("3Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices = p2p.Install(wifiNodes.Get(1), hexoskinNodes.Get(0));  // Hexoskin smartphone to Hexoskin Shirt
    Ipv4AddressHelper p2pAddress;
    p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);
    
    // Define the Baxter Infusion Pump (as a PacketSink on node 0)
    uint16_t baxterPort = 8080;
    Address baxterAddress(InetSocketAddress(wifiInterfaces.GetAddress(0), baxterPort));
    PacketSinkHelper baxterSink("ns3::UdpSocketFactory", baxterAddress);
    ApplicationContainer baxterApp = baxterSink.Install(wifiNodes.Get(0));
    baxterApp.Start(Seconds(1.0));
    baxterApp.Stop(Seconds(20.0));

    // Generate traffic from Hexoskin smartphone (node 1) to Baxter Pump (node 0)
    OnOffHelper baxterTraffic("ns3::UdpSocketFactory", baxterAddress);
    baxterTraffic.SetAttribute("DataRate", StringValue("1Mbps"));
    baxterTraffic.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer baxterTrafficApp = baxterTraffic.Install(wifiNodes.Get(1));  // Hexoskin smartphone
    baxterTrafficApp.Start(Seconds(2.0));
    baxterTrafficApp.Stop(Seconds(20.0));

    // Define the smartphone port for communication
    uint16_t smartphonePort = 9090;
    Address smartphoneAddress(InetSocketAddress(wifiInterfaces.GetAddress(1), smartphonePort));
    PacketSinkHelper smartphoneSink("ns3::UdpSocketFactory", smartphoneAddress);
    ApplicationContainer smartphoneApp = smartphoneSink.Install(wifiNodes.Get(1));  // Hexoskin smartphone
    smartphoneApp.Start(Seconds(2.0));
    smartphoneApp.Stop(Seconds(20.0));

    // Generate traffic from Hexoskin Shirt to Hexoskin smartphone
    OnOffHelper smartphoneTraffic("ns3::UdpSocketFactory", smartphoneAddress);
    smartphoneTraffic.SetAttribute("DataRate", StringValue("512Kbps"));
    smartphoneTraffic.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer smartphoneTrafficApp = smartphoneTraffic.Install(hexoskinNodes.Get(0));  // Hexoskin Shirt
    smartphoneTrafficApp.Start(Seconds(3.0));
    smartphoneTrafficApp.Stop(Seconds(20.0));

    // Initialize blockchain on each node
    Blockchain blockchain;
    
   // Example of adding a new block to the blockchain after an event
    blockchain.addBlock("Baxter Pump data received");
    blockchain.addBlock("Smartphone data received");

    // Print blockchain
    blockchain.printChain();

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configuring secure blockchain communication
    SecureCommunication secureComm;
    secureComm.sendSecureData(wifiNodes, "Block Data");
    secureComm.receiveSecureData("Encrypted(Block Data)");
    
        // Set up the MITM-based blackhole attack
    SetupMitmAttack(attackerNode, wifiNodes, wifiInterfaces);

    // Enable PCAP tracing to capture packet traces
    
    // Enable PCAP collection for Wireless Accesspoint (apDevice.Get(0))  
    phy.EnablePcap("network_blocksec_black", apDevice.Get(0));

   // Enable PCAP collection for Baxter Infusion Pump (wifiNodes.Get(0))
   phy.EnablePcap("baxter-infusion-pump_blocksec_black", wifiDevices.Get(0));

   // Enable PCAP collection for Hexoskin Shirt (p2pDevices.Get(0))
   p2p.EnablePcap("hexoskin-shirt_blocksec_black", p2pDevices.Get(0));

   // Enable PCAP collection for Hexoskin Smartphone (wifiNodes.Get(1))
   phy.EnablePcap("hexoskin-smartphone_blocksec_black", wifiDevices.Get(1));

    // Create animation interface for visualizing the simulation
    AnimationInterface anim("network-anim_blocksec_black.xml");

    // Set up flow monitoring to track packet loss and statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Check for lost packets and serialize flow monitor data
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flowmonitor-stats_blocksec_black.xml", true, true);

    // Display flow statistics
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        std::cout << "Flow ID: " << iter->first << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000 << " kbps" << std::endl;
    }
    
    // Clean up and destroy the simulation
    Simulator::Destroy();

    return 0;
}

