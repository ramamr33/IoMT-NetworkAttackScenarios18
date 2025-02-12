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

NS_LOG_COMPONENT_DEFINE("IoMTWithMITM");

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

NodeContainer mitmNode;

// MITM node callback function to inspect and modify packets
void MITMNodeRxCallback(Ptr<const Packet> packet) {
    std::cout << "MITM Node received a packet of size " << packet->GetSize() << " bytes" << std::endl;

    if (packet->GetSize() > 512) {
        std::cout << "MITM Node dropping a large packet." << std::endl;
        return;
    }

    uint8_t *data = new uint8_t[packet->GetSize()];
    packet->CopyData(data, packet->GetSize());
    data[0] = 0xAB;  // Modify the first byte

    Ptr<Packet> modifiedPacket = Create<Packet>(data, packet->GetSize());

    Ptr<NetDevice> mitmDevice = mitmNode.Get(0)->GetDevice(0);
    mitmDevice->Send(modifiedPacket, mitmDevice->GetAddress(), 0);

    delete[] data;
}

int main(int argc, char *argv[]) {
    double simulationTime = 40.0; // seconds
    uint32_t numWifiNodes = 9;

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create Wi-Fi nodes and AP
    NodeContainer wifiNodes;
    wifiNodes.Create(numWifiNodes);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    mitmNode.Create(1); // Create the MITM node

    // Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("IoMTNetwork");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Configure MITM device
    NetDeviceContainer mitmDevice = wifi.Install(phy, mac, mitmNode);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);
    stack.Install(wifiApNode);
    stack.Install(mitmNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer mitmInterface = address.Assign(mitmDevice);

    // Mobility configuration
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // AP mobility
    Ptr<ListPositionAllocator> apPosition = CreateObject<ListPositionAllocator>();
    apPosition->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPosition);
    mobility.Install(wifiApNode);

    // MITM mobility
    Ptr<ListPositionAllocator> mitmPosition = CreateObject<ListPositionAllocator>();
    mitmPosition->Add(Vector(15.0, 15.0, 0.0));
    mobility.SetPositionAllocator(mitmPosition);
    mobility.Install(mitmNode);

    // Baxter Infusion Pump
    uint16_t baxterPort = 8080;
    Address baxterAddress(InetSocketAddress(staInterfaces.GetAddress(0), baxterPort));
    PacketSinkHelper baxterSink("ns3::UdpSocketFactory", baxterAddress);
    ApplicationContainer baxterApp = baxterSink.Install(wifiNodes.Get(0));
    baxterApp.Start(Seconds(1.0));
    baxterApp.Stop(Seconds(40.0));

    OnOffHelper baxterTraffic("ns3::UdpSocketFactory", baxterAddress);
    baxterTraffic.SetAttribute("DataRate", StringValue("1Mbps"));
    baxterTraffic.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer baxterTrafficApp = baxterTraffic.Install(wifiNodes.Get(1));
    baxterTrafficApp.Start(Seconds(2.0));
    baxterTrafficApp.Stop(Seconds(40.0));

    // Hexoskin smartphone
    uint16_t hexoskinPort = 8090;
    Address hexoskinAddress(InetSocketAddress(staInterfaces.GetAddress(1), hexoskinPort));
    PacketSinkHelper hexoskinSink("ns3::UdpSocketFactory", hexoskinAddress);
    ApplicationContainer hexoskinApp = hexoskinSink.Install(wifiNodes.Get(1));
    hexoskinApp.Start(Seconds(5.0));
    hexoskinApp.Stop(Seconds(40.0));

    // Hexoskin shirt (Target Node)
    OnOffHelper hexoskinTraffic("ns3::UdpSocketFactory", hexoskinAddress);
    hexoskinTraffic.SetAttribute("DataRate", StringValue("500kbps"));
    hexoskinTraffic.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer hexoskinTrafficApp = hexoskinTraffic.Install(wifiNodes.Get(2));
    hexoskinTrafficApp.Start(Seconds(6.0));
    hexoskinTrafficApp.Stop(Seconds(40.0));

    Blockchain blockchain;
    blockchain.addBlock("Baxter Pump data received");
    blockchain.addBlock("Smartphone data received");
    blockchain.printChain();
    
    SecureCommunication secureComm;
    secureComm.sendSecureData(wifiNodes, "Block Data");
    secureComm.receiveSecureData("Encrypted(Block Data)");
    
    // Connect MITM callback for the Hexoskin shirt
    Config::ConnectWithoutContext("/NodeList/10/DeviceList/0/Mac/MacRx", MakeCallback(&MITMNodeRxCallback));

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable PCAP tracing
    phy.EnablePcap("wifi-ap_mitm", apDevice);
    phy.EnablePcap("baxter_mitm", staDevices.Get(0));
    phy.EnablePcap("hexoskin_phone_mitm", staDevices.Get(1));
    phy.EnablePcap("hexoskin_shirt_mitm", staDevices.Get(2));
    phy.EnablePcap("mitm_node", mitmDevice);

    // NetAnim
    AnimationInterface anim("network-anim_mitm.xml");

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // FlowMonitor statistics
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flowmonitor-stats_mitm.xml", true, true);

    // Display flow statistics
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        std::cout << "Flow ID: " << iter->first << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000 << " kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}

