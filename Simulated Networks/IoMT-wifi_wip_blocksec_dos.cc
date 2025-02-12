#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
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

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);  // Log flow monitor info

    // Set simulation parameters
    double simulationTime = 30.0; // seconds
    
    // Number of nodes
    uint32_t numNodes = 9; // 9 Wi-Fi devices

    // Create Wi-Fi nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(numNodes);

    // Create a wireless access point (AP) node
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // Single AP

    // Create Hexoskin Shirt Node (Bluetooth emulation)
    NodeContainer hexoskinNodes;
    hexoskinNodes.Create(1);

    // Wi-Fi channel and PHY configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = ns3::YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi for station nodes
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("HealthNet_24G");

    // Configure station (STA) MAC
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer wifiDevices = wifi.Install(phy, mac, wifiNodes);

    // Configure AP MAC
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install IP stack on all nodes
    InternetStackHelper stack;
    stack.Install(wifiNodes);
    stack.Install(wifiApNode);
    stack.Install(hexoskinNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Set up mobility for station nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Set up mobility for the AP
    Ptr<ListPositionAllocator> apPosition = CreateObject<ListPositionAllocator>();
    apPosition->Add(Vector(0.0, 0.0, 0.0)); // AP fixed at (0, 0)
    mobility.SetPositionAllocator(apPosition);
    mobility.Install(wifiApNode);

    // Set up mobility for the Hexoskin Shirt
    mobility.Install(hexoskinNodes);

    // Simulate Bluetooth with Point-to-Point Link between Smartphone and Hexoskin Shirt
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("3Mbps")); // Bluetooth typical speed
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices = p2p.Install(wifiNodes.Get(1), hexoskinNodes.Get(0));
    Ipv4AddressHelper p2pAddress;
    p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

    // Initialize blockchain on each node
    Blockchain blockchain;

    // Baxter Pump Traffic (High Priority)
    uint16_t baxterPort = 8080;
    Address baxterAddress(InetSocketAddress(wifiInterfaces.GetAddress(0), baxterPort));
    PacketSinkHelper baxterSink("ns3::UdpSocketFactory", baxterAddress);
    ApplicationContainer baxterApp = baxterSink.Install(wifiNodes.Get(0)); // Baxter node
    baxterApp.Start(Seconds(1.0));
    baxterApp.Stop(Seconds(20.0));

    OnOffHelper baxterTraffic("ns3::UdpSocketFactory", baxterAddress);
    baxterTraffic.SetAttribute("DataRate", StringValue("1Mbps"));
    baxterTraffic.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer baxterTrafficApp = baxterTraffic.Install(wifiNodes.Get(2)); // A secondary control device
    baxterTrafficApp.Start(Seconds(2.0));
    baxterTrafficApp.Stop(Seconds(20.0));

    // Smartphone Traffic (Lower Priority)
    uint16_t smartphonePort = 9090;
    Address smartphoneAddress(InetSocketAddress(wifiInterfaces.GetAddress(1), smartphonePort));
    PacketSinkHelper smartphoneSink("ns3::UdpSocketFactory", smartphoneAddress);
    ApplicationContainer smartphoneApp = smartphoneSink.Install(wifiNodes.Get(1)); // Smartphone node
    smartphoneApp.Start(Seconds(2.0));
    smartphoneApp.Stop(Seconds(20.0));

    OnOffHelper smartphoneTraffic("ns3::UdpSocketFactory", smartphoneAddress);
    smartphoneTraffic.SetAttribute("DataRate", StringValue("512Kbps"));
    smartphoneTraffic.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer smartphoneTrafficApp = smartphoneTraffic.Install(hexoskinNodes.Get(0)); // Hexoskin Shirt
    smartphoneTrafficApp.Start(Seconds(3.0));
    smartphoneTrafficApp.Stop(Seconds(20.0));

    // Example of adding a new block to the blockchain after an event
    blockchain.addBlock("Baxter Pump data received");
    blockchain.addBlock("Smartphone data received");

    // Print blockchain
    blockchain.printChain();

    // Enable routing on the relay node so it can forward packets
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    wifiNodes.Get(0)->GetObject<Ipv4>()->SetForwarding(1, true);  // Enable forwarding on interface 1 of relay
	
    //Configuring Secure Blockchain Communication
    SecureCommunication secureComm;
    secureComm.sendSecureData(wifiNodes, "Block Data");
    secureComm.receiveSecureData("Encrypted(Block Data)");
    
Ptr<Node> attackerNode = wifiDevices.Get(8)->GetNode(); // Attacker node
Ptr<Node> targetNode = wifiNodes.Get(0);   // Target node (Baxter Infusion Pump)

// Fetch the IP address of the target node
Ipv4Address targetAddress = wifiInterfaces.GetAddress(0);  // Get target node's IP address

// Configure the UDP Echo Client for the DoS attack
UdpEchoClientHelper attackClient(targetAddress, 9);
attackClient.SetAttribute("MaxPackets", UintegerValue(1000000));
attackClient.SetAttribute("Interval", TimeValue(Seconds(0.01))); // Flood rate
attackClient.SetAttribute("PacketSize", UintegerValue(1024));

ApplicationContainer attackApp = attackClient.Install(attackerNode);
attackApp.Start(Seconds(1.0));
attackApp.Stop(Seconds(40.0));
    
    NS_LOG_INFO("Attacker Node: " << attackerNode->GetId());
    NS_LOG_INFO("Target Node: " << targetNode->GetId());

    // Setup Pcap capturing for devices
    phy.EnablePcap("wifi_ap_blocksec_dos", wifiApNode); // Capture WAP traffic
    phy.EnablePcap("baxter_pump_blocksec_dos", wifiDevices.Get(0)); // Capture Baxter traffic (Node 0)
    phy.EnablePcap("hexoskin_phone_blocksec_dos", wifiDevices.Get(1)); // Capture Hexoskin traffic (Node 1)
    p2p.EnablePcap("hexoskin_shirt_blocksec_dos", p2pDevices); // Hexoskin Shirt device (second device in p2pDevices)

    // Enable NetAnim trace
    AnimationInterface anim ("network-anim_blocksec_dos.xml");

    // FlowMonitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime)); // Extended time
    Simulator::Run();

    // Check for lost packets and log stats
    monitor->CheckForLostPackets();

    // Save FlowMonitor stats to XML (for graphing)
    monitor->SerializeToXmlFile("flowmonitor-stats_blocksec_dos.xml", true, true);
    
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
    
    // Clean up and exit
    Simulator::Destroy();

    return 0;
}

