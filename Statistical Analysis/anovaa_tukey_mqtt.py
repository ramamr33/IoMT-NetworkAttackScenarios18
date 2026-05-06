import xml.etree.ElementTree as ET
import pandas as pd
import scipy.stats as stats
from statsmodels.stats.multicomp import pairwise_tukeyhsd
import numpy as np

# --- Step 1: Parse XML ---
tree = ET.parse("flowmonitor-stats_wip_blocksec_mqtt.xml")
root = tree.getroot()

flows_data = []
for flow in root.findall(".//FlowStats/Flow"):
    fid = int(flow.attrib["flowId"])
    tx_bytes = int(flow.attrib["txBytes"])
    rx_bytes = int(flow.attrib["rxBytes"])
    tx_packets = int(flow.attrib["txPackets"])
    rx_packets = int(flow.attrib["rxPackets"])
    lost_packets = int(flow.attrib["lostPackets"])
    delay_sum_ns = int(flow.attrib["delaySum"].replace("+", "").replace("ns", ""))
    jitter_sum_ns = int(flow.attrib["jitterSum"].replace("+", "").replace("ns", ""))
    time_first_tx_ns = int(flow.attrib["timeFirstTxPacket"].replace("+", "").replace("ns", ""))
    time_last_rx_ns = int(flow.attrib["timeLastRxPacket"].replace("+", "").replace("ns", ""))

    duration_s = (time_last_rx_ns - time_first_tx_ns) / 1e9 if time_last_rx_ns > time_first_tx_ns else 1e-9

    throughput_kbps = (rx_bytes * 8) / (duration_s * 1000)
    delay_s = delay_sum_ns / 1e9 / rx_packets if rx_packets > 0 else 0
    jitter_s = jitter_sum_ns / 1e9 / (rx_packets - 1) if rx_packets > 1 else 0
    packet_loss_pct = (lost_packets / (rx_packets + lost_packets)) * 100 if (rx_packets + lost_packets) > 0 else 0

    flows_data.append({
        "FlowID": fid,
        "Throughput_kbps": throughput_kbps,
        "Delay_s": delay_s,
        "Jitter_s": jitter_s,
        "PacketLoss_pct": packet_loss_pct
    })

df = pd.DataFrame(flows_data)

# Optional: assign groups (e.g., SHS, WIP, Attacked Broker) for statistical comparison
# You can update this mapping based on your scenario design
group_map = {
    1: "WIP", 2: "SHS", 3: "SHS", 4: "Broker", 5: "Node", 6: "Node", 7: "Node", 8: "Node", 9: "Node", 10: "Node", 11: "WIP"
}
df["Group"] = df["FlowID"].map(group_map).fillna("Other")

# --- Step 2: ANOVA & Tukey HSD ---
def perform_anova_and_tukey(metric_name):
    print(f"\n--- {metric_name.upper()} ---")
    groups = [group[metric_name].values for name, group in df.groupby("Group")]
    f_stat, p_val = stats.f_oneway(*groups)
    print(f"ANOVA F-statistic: {f_stat:.4f}, p-value: {p_val:.4e}")

    # Tukey HSD
    tukey = pairwise_tukeyhsd(endog=df[metric_name], groups=df["Group"], alpha=0.05)
    print(tukey)

metrics = ["Throughput_kbps", "Delay_s", "Jitter_s", "PacketLoss_pct"]
for metric in metrics:
    perform_anova_and_tukey(metric)

# Save data for reference
df.to_csv("flow_metrics_extracted.csv", index=False)

