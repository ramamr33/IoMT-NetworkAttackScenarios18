import os
import glob
import pandas as pd
from lxml import etree
from scipy.stats import f_oneway, t
from statsmodels.stats.multicomp import pairwise_tukeyhsd
import statsmodels.api as sm
import numpy as np

# Config
DATA_DIR = "data2"
SCENARIOS = ["normal", "mitm"]
ANOVA_OUT = "anova_results_fl2.csv"
TUKEY_OUT = "tukey_hsd_results_fl2.csv"

# === Helper: Parse time strings like '+0ns', '10ms', '1.5s'
def parse_time_to_seconds(time_str):
    try:
        if time_str.startswith('+'):
            time_str = time_str[1:]
        if time_str.endswith('ns'):
            return float(time_str.replace('ns', '')) * 1e-9
        elif time_str.endswith('us'):
            return float(time_str.replace('us', '')) * 1e-6
        elif time_str.endswith('ms'):
            return float(time_str.replace('ms', '')) * 1e-3
        elif time_str.endswith('s'):
            return float(time_str.replace('s', ''))
        else:
            return float(time_str)
    except:
        return 0.0

# === Helper: Parse XML to extract metrics
def extract_metrics_from_xml(file_path):
    metrics = []
    try:
        tree = etree.parse(file_path)
        root = tree.getroot()
        flow_elements = root.xpath('//FlowStats/Flow')
        print(f"Parsed {len(flow_elements)} flows from {file_path}")

        for flow in flow_elements:
            fid = int(flow.attrib['flowId'])
            txBytes = int(flow.attrib['txBytes'])
            rxBytes = int(flow.attrib['rxBytes'])
            delaySum = parse_time_to_seconds(flow.attrib.get('delaySum', '0s'))
            jitterSum = parse_time_to_seconds(flow.attrib.get('jitterSum', '0s'))
            rxPackets = int(flow.attrib['rxPackets'])
            txPackets = int(flow.attrib['txPackets'])
            t_first = parse_time_to_seconds(flow.attrib.get('timeFirstTxPacket', '0s'))
            t_last = parse_time_to_seconds(flow.attrib.get('timeLastRxPacket', '0s'))
            duration = t_last - t_first if t_last > t_first else 0.0

            throughput = (rxBytes * 8) / duration / 1e6 if duration > 0 else 0
            e2e_delay = (delaySum / rxPackets) * 1000 if rxPackets > 0 else 0
            jitter = (jitterSum / rxPackets) * 1000 if rxPackets > 0 else 0
            packet_loss = ((txPackets - rxPackets) / txPackets) * 100 if txPackets > 0 else 0

            metrics.append({
                "FlowID": fid,
                "Throughput(Mbps)": throughput,
                "EndToEndDelay(ms)": e2e_delay,
                "Jitter(ms)": jitter,
                "PacketLoss(%)": packet_loss
            })
    except Exception as e:
        print(f"Error parsing {file_path}: {e}")
    return metrics

# === Helper: Compute mean and 95% CI
def mean_ci(data):
    n = len(data)
    mean = np.mean(data)
    std = np.std(data, ddof=1)
    ci95 = t.ppf(0.975, n - 1) * (std / np.sqrt(n)) if n > 1 else 0
    return mean, mean - ci95, mean + ci95

# === Step 1: Load all FlowMonitor data
all_data = []

for scenario in SCENARIOS:
    path = os.path.join(DATA_DIR, scenario, "*.xml")
    for file in sorted(glob.glob(path)):
        rows = extract_metrics_from_xml(file)
        for row in rows:
            row["Scenario"] = scenario
            all_data.append(row)

df = pd.DataFrame(all_data)
print("\n✅ Total valid rows loaded:", len(df))

if df.empty:
    print("❌ No data to analyze.")
    exit()

# === Step 2: Perform ANOVA and collect means + CI
anova_rows = []
tukey_rows = []
metrics = ["Throughput(Mbps)", "EndToEndDelay(ms)", "Jitter(ms)", "PacketLoss(%)"]

for metric in metrics:
    print(f"\n=== Analyzing: {metric} ===")

    # Prepare groups
    group_data = []
    group_names = []
    means_ci = []

    for scen in SCENARIOS:
        values = df[df["Scenario"] == scen][metric].dropna()
        if len(values) > 1:
            mean, ci_lower, ci_upper = mean_ci(values)
            means_ci.append({
                "Metric": metric,
                "Scenario": scen,
                "Mean": mean,
                "95CI_Lower": ci_lower,
                "95CI_Upper": ci_upper
            })
            group_data.append(values)
            group_names.append(scen)
        else:
            print(f"⚠️ Not enough data for scenario: {scen} on metric: {metric}")

    # ANOVA
    try:
        anova_result = f_oneway(*group_data)
        print(f"F = {anova_result.statistic:.4f}, p = {anova_result.pvalue:.4e}")

        for row in means_ci:
            row.update({
                "F-statistic": anova_result.statistic,
                "P-value": anova_result.pvalue
            })
            anova_rows.append(row)

        # Tukey HSD
        tukey_result = pairwise_tukeyhsd(df[metric], df["Scenario"])
        tukey_df = pd.DataFrame(tukey_result.summary().data[1:], columns=tukey_result.summary().data[0])

        # Map group means
        mean_map = {r['Scenario']: r['Mean'] for r in means_ci}
        for _, row in tukey_df.iterrows():
            g1, g2 = row['group1'], row['group2']
            tukey_rows.append({
                "Metric": metric,
                "Group1": g1,
                "Group2": g2,
                "Mean1": mean_map.get(g1, np.nan),
                "Mean2": mean_map.get(g2, np.nan),
                "MeanDiff": row['meandiff'],
                "p-adj": row['p-adj'],
                "CI Lower": row['lower'],
                "CI Upper": row['upper'],
                "Reject": row['reject']
            })
        print(tukey_result)

    except Exception as e:
        print(f"❌ Error analyzing {metric}: {e}")

# === Step 3: Save results
pd.DataFrame(anova_rows).to_csv(ANOVA_OUT, index=False)
pd.DataFrame(tukey_rows).to_csv(TUKEY_OUT, index=False)

print("\n✅ ANOVA output:", ANOVA_OUT)
print("✅ Tukey HSD output:", TUKEY_OUT)

