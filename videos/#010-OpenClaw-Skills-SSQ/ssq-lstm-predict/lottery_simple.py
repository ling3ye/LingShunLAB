import pandas as pd
import numpy as np
import os
import sys
import json

# ==================== 配置区 ====================
CSV_PATH = "/Users/shawn/.openclaw/workspace/skills/ssq-lstm-predict/ssq.csv"

# ==================== 1. 读取 + 强制最旧→最新 ====================
def load_data():
    if not os.path.exists(CSV_PATH):
        print(json.dumps({
            'success': False,
            'error': f'CSV file not found at {CSV_PATH}'
        }, ensure_ascii=False))
        sys.exit(1)
    
    df = pd.read_csv(CSV_PATH)
    red_cols = ['ball1', 'ball2', 'ball3', 'ball4', 'ball5', 'ball6']
    df['reds'] = df[red_cols].values.tolist()
    df['reds'] = df['reds'].apply(lambda x: sorted([int(n) for n in x]))
    df['blue'] = df['special_ball'].astype(int)
    
    df = df.sort_values(by='issue_no', ascending=True).reset_index(drop=True)
    return df

# ==================== 2. 遗漏统计 ====================
def calculate_miss_data(df):
    period_count = len(df)
    
    red_miss = {}
    for num in range(1, 34):
        last_appear = None
        for i in range(period_count - 1, -1, -1):
            if num in df.iloc[i]['reds']:
                last_appear = period_count - 1 - i
                break
        red_miss[num] = last_appear if last_appear is not None else period_count
    
    blue_miss = {}
    for num in range(1, 17):
        last_appear = None
        for i in range(period_count - 1, -1, -1):
            if df.iloc[i]['blue'] == num:
                last_appear = period_count - 1 - i
                break
        blue_miss[num] = last_appear if last_appear is not None else period_count
    
    return red_miss, blue_miss

def calculate_stats(df):
    period_count = len(df)
    latest = df.iloc[-1]
    
    red_miss, blue_miss = calculate_miss_data(df)
    latest_mean = np.mean(latest['reds'])
    
    hot_numbers = [k for k, v in red_miss.items() if v == 0]
    cold_numbers = [k for k, v in red_miss.items() if v >= 10]
    
    return {
        'latestMean': float(latest_mean),
        'hotNumbers': sorted(hot_numbers),
        'coldNumbers': sorted(cold_numbers),
        'redMaxMiss': int(max(red_miss.values())),
        'blueMaxMiss': int(max(blue_miss.values())),
        'periodCount': period_count
    }

# ==================== 3. 简单预测（基于热号+冷号+概率） ====================
def simple_predict(df, red_miss, blue_miss):
    # 简单预测逻辑：结合热号、冷号和随机因素
    period_count = len(df)
    
    # 计算每个红球的历史出现频率
    red_freq = {i: 0 for i in range(1, 34)}
    blue_freq = {i: 0 for i in range(1, 17)}
    
    for _, row in df.iterrows():
        for num in row['reds']:
            red_freq[num] += 1
        blue_freq[row['blue']] += 1
    
    # 基于频率和遗漏计算权重
    def get_red_weight(num):
        freq_score = red_freq[num] / period_count
        # 遗漏适中的数字权重更高（既不是刚出也不是太久）
        miss_score = 1 / (1 + abs(red_miss[num] - 3))
        return freq_score * 0.7 + miss_score * 0.3
    
    def get_blue_weight(num):
        freq_score = blue_freq[num] / period_count
        miss_score = 1 / (1 + abs(blue_miss[num] - 2))
        return freq_score * 0.7 + miss_score * 0.3
    
    # 加权随机选择红球
    red_weights = {i: get_red_weight(i) for i in range(1, 34)}
    red_numbers = list(range(1, 34))
    red_probs = [red_weights[i] for i in red_numbers]
    red_probs = np.array(red_probs) / np.sum(red_probs)
    red_pred = np.random.choice(red_numbers, size=6, replace=False, p=red_probs)
    red_pred = sorted(red_pred.tolist())
    
    # 加权随机选择蓝球
    blue_weights = {i: get_blue_weight(i) for i in range(1, 17)}
    blue_numbers = list(range(1, 17))
    blue_probs = [blue_weights[i] for i in blue_numbers]
    blue_probs = np.array(blue_probs) / np.sum(blue_probs)
    blue_pred = int(np.random.choice(blue_numbers, p=blue_probs))
    
    return red_pred, blue_pred

# ==================== API 接口 ====================
def main():
    command = sys.argv[1] if len(sys.argv) > 1 else 'stats'
    
    df = load_data()
    
    if command == 'miss':
        red_miss, blue_miss = calculate_miss_data(df)
        result = {
            'success': True,
            'redMiss': {str(k): v for k, v in red_miss.items()},
            'blueMiss': {str(k): v for k, v in blue_miss.items()}
        }
    elif command == 'predict':
        red_miss, blue_miss = calculate_miss_data(df)
        red_pred, blue_pred = simple_predict(df, red_miss, blue_miss)
        result = {
            'success': True,
            'prediction': {
                'red': red_pred,
                'blue': blue_pred
            }
        }
    else:  # stats
        stats = calculate_stats(df)
        result = {
            'success': True,
            'stats': stats
        }
    
    print(json.dumps(result, ensure_ascii=False))

if __name__ == '__main__':
    main()
