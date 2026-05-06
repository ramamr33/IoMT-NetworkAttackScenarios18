import os
import glob
import xmltodict
import pandas as pd
import numpy as np
from scipy import stats
from statsmodels.stats.multicomp import pairwise_tukeyhsd
import warnings

warnings.filterwarnings("ignore", category=RuntimeWarning)

base_dir = 'data'
scenarios = ['normal', 'dos', 'ddos', 'mitm', 'blackhole', 'mqtt']
metrics = ["Throughput_Mbps", "End-to-End_Delay_s", "Jitter_s", "Packet_Loss_%"]
records = []

def clean_ns(value):
    if isinstance(value, str):
        return float(value.replace('ns', '').replace('+', '').strip())
    return float(value)

def extract_metrics_from_xml(xml_file):
    with open(xml_file) as f:
        doc = xmltodict.parse(f.read())

    flows = doc['FlowMonitor']['FlowStats']['Flow']
    if isinstance(flows, dict):
        flows = [flows]

    total_rx_bytes = 0
    total_tx_bytes = 0
    total_delay = 0
    total_jitter = 0
    total_lost_packets = 0
    total_received_packets = 0
    throughput_list = []
    valid_flows_count = 0

    for flow in flows:
        flow = {k.lstrip('@'): v for k, v in flow.items()}
        required_keys = [
            'txBytes', 'rxBytes', 'delaySum', 'jitterSum',
            'lostPackets', 'rxPackets', 'timeFirstTxPacket', 'timeLastRxPacket'
        ]
        if any(k not in flow for k in required_keys):
            continue

        try:
            tx_bytes = int(flow['txBytes'])
            rx_bytes = int(flow['rxBytes'])
            delay_sum = clean_ns(flow['delaySum']) / 1e9
            jitter_sum = clean_ns(flow['jitterSum']) / 1e9
            lost_packets = int(flow['lostPackets'])
            rx_packets = int(flow['rxPackets'])
            time_first = clean_ns(flow['timeFirstTxPacket'])
            time_last = clean_ns(flow['timeLastRxPacket'])
            duration = time_last - time_first

            throughput = (rx_bytes * 8) / (duration * 1e6) if duration > 0 else 0
            throughput_list.append(throughput)

            total_rx_bytes += rx_bytes
            total_tx_bytes += tx_bytes
            total_delay += delay_sum
            total_jitter += jitter_sum
            total_lost_packets += lost_packets
            total_received_packets += rx_packets
            valid_flows_count += 1
        except Exception:
            continue

    if valid_flows_count == 0:
        return {m: np.nan for m in metrics}

    throughput_avg = np.mean(throughput_list) if throughput_list else np.nan
    delay_avg = total_delay / valid_flows_count
    jitter_avg = total_jitter / valid_flows_count
    packet_loss_ratio = (total_lost_packets / (total_lost_packets + total_received_packets)) * 100 \
        if (total_lost_packets + total_received_packets) > 0 else 0

    return {
        "Throughput_Mbps": throughput_avg,
        "End-to-End_Delay_s": delay_avg,
        "Jitter_s": jitter_avg,
        "Packet_Loss_%": packet_loss_ratio
    }

# Parse all XML files
for scenario in scenarios:
    scenario_path = os.path.join(base_dir, scenario)
    xml_files = glob.glob(os.path.join(scenario_path, '*.xml'))

    if not xml_files:
        print(f"[WARNING] No XML files found for scenario '{scenario}'")
        continue

    for xml_file in xml_files:
        metrics_result = extract_metrics_from_xml(xml_file)
        metrics_result["Scenario"] = scenario
        metrics_result["Seed"] = os.path.basename(xml_file)
        records.append(metrics_result)

df = pd.DataFrame(records)
for metric in metrics:
    df[metric] = pd.to_numeric(df[metric], errors='coerce')

valid_scenarios = df["Scenario"].unique()

print("\nStandard deviation per scenario and metric:")
for metric in metrics:
    stds = df.groupby("Scenario")[metric].std()
    counts = df.groupby("Scenario")[metric].count()
    print(f"\nMetric: {metric}")
    print(pd.DataFrame({"std_dev": stds, "count": counts}))

# Statistical analysis containers
anova_results = {}
tukey_rows = []

for metric in metrics:
    # Filter scenarios with at least 2 samples and non-zero variance
    filtered_scenarios = []
    for scenario in valid_scenarios:
        values = df[df["Scenario"] == scenario][metric].dropna()
        if len(values) > 1 and values.std() > 0:
            filtered_scenarios.append(scenario)
    if len(filtered_scenarios) < 2:
        print(f"[WARNING] Skipping ANOVA and Tukey HSD for {metric}: Not enough valid groups with variance.")
        anova_results[metric] = np.nan
        continue

    # Prepare groups for ANOVA
    groups = [df[df["Scenario"] == s][metric].dropna() for s in filtered_scenarios]
    try:
        f_stat, p_val = stats.f_oneway(*groups)
        anova_results[metric] = p_val
        print(f"[INFO] ANOVA p-value for {metric}: {p_val:.4f}")
    except Exception as e:
        print(f"[ERROR] ANOVA failed for {metric}: {e}")
        anova_results[metric] = np.nan
        continue

    # Tukey HSD test (only on valid scenarios)
    metric_data = df[df["Scenario"].isin(filtered_scenarios)][[metric, "Scenario"]].dropna()
    try:
        tukey = pairwise_tukeyhsd(endog=metric_data[metric], groups=metric_data["Scenario"], alpha=0.05)
        df_tukey = pd.DataFrame(tukey.summary().data[1:], columns=tukey.summary().data[0])
        df_tukey.insert(0, "Metric", metric)
        tukey_rows.append(df_tukey)
        print(f"[INFO] Tukey HSD performed for {metric}")
    except Exception as e:
        print(f"[ERROR] Tukey HSD failed for {metric}: {e}")

# Save Tukey HSD results
if tukey_rows:
    tukey_all_df = pd.concat(tukey_rows, ignore_index=True)
    tukey_all_df.to_csv("tukey_hsd_structured_results.csv", index=False)
    print("[INFO] Saved Tukey HSD results to tukey_hsd_structured_results.csv")
else:
    print("[INFO] No valid Tukey HSD results to save.")

# Compute means and 95% confidence intervals per metric and scenario
CI_LEVEL = 0.95
confidence_intervals = {}
means = {}

for metric in metrics:
    ci_dict = {}
    mean_dict = {}
    for scenario in valid_scenarios:
        values = df[df["Scenario"] == scenario][metric].dropna()
        mean = np.mean(values) if len(values) > 0 else np.nan
        if len(values) < 2 or np.std(values) == 0:
            ci = (np.nan, np.nan)
        else:
            sem = stats.sem(values)
            ci = stats.t.interval(CI_LEVEL, len(values)-1, loc=mean, scale=sem)
        ci_dict[scenario] = ci
        mean_dict[scenario] = mean
    confidence_intervals[metric] = ci_dict
    means[metric] = mean_dict

results_df = pd.DataFrame({
    "Metric": metrics,
    "ANOVA_p_value": [anova_results.get(m, np.nan) for m in metrics],
    "Confidence_Intervals": [confidence_intervals[m] for m in metrics],
    "Means": [means[m] for m in metrics]
})

results_df.to_csv("p_values_confidence_intervals.csv", index=False)
print("[INFO] Saved ANOVA and Confidence Intervals to p_values_confidence_intervals.csv")

df.to_csv("raw_metrics_all_scenarios.csv", index=False)
print("[INFO] Saved raw metrics to raw_metrics_all_scenarios.csv")
