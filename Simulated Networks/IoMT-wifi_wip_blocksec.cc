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
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);

    double simulationTime = 30.0;
    uint32_t numNodes = 9;

    NodeContainer wifiNodes;
    wifiNodes.Create(numNodes);

    NodeContainer wifiApNode;
    wifiApNode.Create(1); 

    NodeContainer hexoskinNodes;
    hexoskinNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = ns3::YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("HealthNet_24G");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer wifiDevices = wifi.Install(phy, mac, wifiNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiNodes);
    stack.Install(wifiApNode);
    stack.Install(hexoskinNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

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

    Ptr<ListPositionAllocator> apPosition = CreateObject<ListPositionAllocator>();
    apPosition->Add(Vector(0.0, 0.0, 0.0)); 
    mobility.SetPositionAllocator(apPosition);
    mobility.Install(wifiApNode);

    mobility.Install(hexoskinNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("3Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices = p2p.Install(wifiNodes.Get(1), hexoskinNodes.Get(0));
    Ipv4AddressHelper p2pAddress;
    p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

    Blockchain blockchain;
    blockchain.addBlock("Baxter Pump data received");
    blockchain.addBlock("Smartphone data received");
    blockchain.printChain();

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    wifiNodes.Get(0)->GetObject<Ipv4>()->SetForwarding(1, true);

    SecureCommunication secureComm;
    secureComm.sendSecureData(wifiNodes, "Block Data");
    secureComm.receiveSecureData("Encrypted(Block Data)");

    // Enable routing and flow monitoring
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Setup Pcap capturing for devices
    phy.EnablePcap("wifi_ap_blocksec", wifiApNode); // Capture WAP traffic
    phy.EnablePcap("baxter_pump_blocksec", wifiDevices.Get(0)); // Capture Baxter traffic (Node 0)
    phy.EnablePcap("hexoskin_phone_blocksec", wifiDevices.Get(1)); // Capture Hexoskin traffic (Node 1)
    p2p.EnablePcap("hexoskin_shirt_blocksec", p2pDevices); // Hexoskin Shirt device (second device in p2pDevices)

    // Enable NetAnim trace
    AnimationInterface anim ("network-anim_blocksec.xml");

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Check for lost packets and log stats
    monitor->CheckForLostPackets();

    // Save FlowMonitor stats to XML (for graphing)
    monitor->SerializeToXmlFile("flowmonitor-stats_blocksec.xml", true, true);
    
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

