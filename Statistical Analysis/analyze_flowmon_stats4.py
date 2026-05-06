import os
import glob
import re
import xmltodict
import pandas as pd
import numpy as np
import scipy.stats as stats
from statsmodels.stats.multicomp import pairwise_tukeyhsd
import matplotlib.pyplot as plt

# Set top-level data directory
base_dir = 'metrics'

# List of scenarios (subfolder names)
scenarios = ['mitm-wip', 'mitm-shs']

# Metrics container
records = []

# Log file for problematic flows
invalid_log_file = "invalid_flows_log.txt"
if os.path.exists(invalid_log_file):
    os.remove(invalid_log_file)

def parse_ns_string(ns_str):
    """Convert time string like '+1.23e+09ns' to float seconds."""
    try:
        return float(re.sub(r'[^\deE.-]', '', ns_str)) / 1e9
    except:
        return 0.0

def extract_metrics_from_xml(xml_file):
    with open(xml_file) as f:
        doc = xmltodict.parse(f.read())

    flows = doc['FlowMonitor']['FlowStats']['Flow']
    if isinstance(flows, dict):  # Only one flow
        flows = [flows]

    total_rx_bytes = 0
    total_tx_bytes = 0
    total_delay = 0
    total_jitter = 0
    total_lost_packets = 0
    total_received_packets = 0
    throughput_list = []

    for flow in flows:
        required_fields = [
            'txBytes', 'rxBytes', 'delaySum', 'jitterSum',
            'lostPackets', 'rxPackets', 'timeLastRxPacket', 'timeFirstTxPacket'
        ]
        if not all(field in flow for field in required_fields):
            with open(invalid_log_file, "a") as log:
                log.write(f"File: {xml_file}\nMissing fields in flow: {flow}\n\n")
            print(f"[WARN] Missing fields in {xml_file} -> Skipping flow.")
            continue

        try:
            tx_bytes = int(flow['txBytes'])
            rx_bytes = int(flow['rxBytes'])
            delay_sum = float(re.sub(r'[^\deE.-]', '', flow['delaySum'])) / 1e9
            jitter_sum = float(re.sub(r'[^\deE.-]', '', flow['jitterSum'])) / 1e9
            lost_packets = int(flow['lostPackets'])
            rx_packets = int(flow['rxPackets'])

            start_time = parse_ns_string(flow['timeFirstTxPacket'])
            end_time = parse_ns_string(flow['timeLastRxPacket'])
            duration = end_time - start_time

            throughput = (rx_bytes * 8) / (duration * 1e6) if duration > 0 else 0
            throughput_list.append(throughput)

            total_rx_bytes += rx_bytes
            total_tx_bytes += tx_bytes
            total_delay += delay_sum
            total_jitter += jitter_sum
            total_lost_packets += lost_packets
            total_received_packets += rx_packets
        except Exception as e:
            print(f"Error parsing flow in {xml_file}: {e}")
            continue

    num_flows = len(throughput_list)
    throughput_avg = np.mean(throughput_list) if num_flows > 0 else 0
    delay_avg = total_delay / num_flows if num_flows > 0 else 0
    jitter_avg = total_jitter / num_flows if num_flows > 0 else 0
    packet_loss_ratio = (
        (total_lost_packets / (total_lost_packets + total_received_packets)) * 100
        if (total_lost_packets + total_received_packets) > 0 else 0
    )

    return {
        "Throughput_Mbps": throughput_avg,
        "End-to-End_Delay_s": delay_avg,
        "Jitter_s": jitter_avg,
        "Packet_Loss_%": packet_loss_ratio
    }

# Loop through scenarios and xml files
for scenario in scenarios:
    scenario_path = os.path.join(base_dir, scenario)
    xml_files = glob.glob(os.path.join(scenario_path, '*.xml'))

    for xml_file in xml_files:
        metrics = extract_metrics_from_xml(xml_file)
        metrics["Scenario"] = scenario
        metrics["Seed"] = os.path.basename(xml_file)

        # Skip if all metrics are zero or NaN
        if all(v == 0 or pd.isna(v) for k, v in metrics.items() if k in ["Throughput_Mbps", "End-to-End_Delay_s", "Jitter_s", "Packet_Loss_%"]):
            print(f"[INFO] Skipping empty/invalid metric entry from {xml_file}")
            continue

        records.append(metrics)

# Load into DataFrame
df = pd.DataFrame(records)

metric_keys = ["Throughput_Mbps", "End-to-End_Delay_s", "Jitter_s", "Packet_Loss_%"]

# ==== MODIFIED BLOCK: Add a safety check for empty DataFrame or missing columns ====
if df.empty or not all(metric in df.columns for metric in metric_keys):
    print("[ERROR] No valid metric data found. All flows were skipped due to missing fields.")
    print(f"Checked {len(records)} metric files. Please check your XML files and invalid_flows_log.txt for details.")
    exit(1)
# ================================================================================

# Convert metrics to numeric
for metric in metric_keys:
    df[metric] = pd.to_numeric(df[metric], errors='coerce')

# Compute confidence intervals and means
confidence_intervals = {}
means = {}
CI_LEVEL = 0.95

for metric in metric_keys:
    ci_dict = {}
    mean_dict = {}
    for scenario in df["Scenario"].unique():
        values = df[df["Scenario"] == scenario][metric].dropna()
        mean = np.mean(values)
        sem = stats.sem(values) if len(values) > 1 else np.nan
        ci = stats.t.interval(CI_LEVEL, len(values)-1, loc=mean, scale=sem) if len(values) > 1 else (np.nan, np.nan)
        ci_dict[scenario] = ci
        mean_dict[scenario] = mean
    confidence_intervals[metric] = ci_dict
    means[metric] = mean_dict

# ANOVA p-values
anova_results = {}
for metric in metric_keys:
    groups = [df[df["Scenario"] == scenario][metric].dropna() for scenario in df["Scenario"].unique()]
    if all(len(g) > 1 for g in groups):
        f_stat, p_val = stats.f_oneway(*groups)
    else:
        p_val = np.nan
    anova_results[metric] = p_val

# Tukey HSD test results
tukey_results = {}
for metric in metric_keys:
    metric_data = df[[metric, "Scenario"]].dropna()
    if metric_data["Scenario"].nunique() > 1:
        tukey = pairwise_tukeyhsd(endog=metric_data[metric], groups=metric_data["Scenario"], alpha=0.05)
        tukey_results[metric] = tukey.summary()

# Save Tukey HSD structured results
tukey_rows = []
for metric, result in tukey_results.items():
    df_tukey = pd.DataFrame(result.data[1:], columns=result.data[0])
    df_tukey.insert(0, "Metric", metric)
    tukey_rows.append(df_tukey)
if tukey_rows:
    tukey_all_df = pd.concat(tukey_rows, ignore_index=True)
    tukey_all_df.to_csv("tukey_hsd_structured_results.csv", index=False)
    print("[INFO] Saved Tukey HSD results to tukey_hsd_structured_results.csv")
else:
    print("[INFO] No Tukey HSD results to save (not enough data for comparison).")

# Save ANOVA and CI results
results_df = pd.DataFrame({
    "Metric": metric_keys,
    "ANOVA_p_value": [anova_results[m] for m in metric_keys],
    "Confidence_Intervals": [confidence_intervals[m] for m in metric_keys],
    "Means": [means[m] for m in metric_keys]
})
results_df.to_csv("p_values_confidence_intervals.csv", index=False)
print("[INFO] Saved ANOVA and Confidence Intervals to p_values_confidence_intervals.csv")
