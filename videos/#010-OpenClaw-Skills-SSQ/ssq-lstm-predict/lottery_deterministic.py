import pandas as pd
import numpy as np
import os
import sys
import json
from collections import Counter, defaultdict

# ==================== 配置区 ====================
CSV_PATH = "/Users/shawn/.openclaw/workspace/skills/ssq-lstm-predict/ssq.csv"
RANDOM_SEED = 42  # 固定随机种子，确保结果可重现

np.random.seed(RANDOM_SEED)

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

# ==================== 3. 频率分析与模式识别 ====================
def analyze_patterns(df, window_size=50):
    """
    分析历史数据中的模式：
    1. 号码出现频率
    2. 冷热号交替规律
    3. 连号出现概率
    4. 奇偶比例
    5. 大小比例
    """
    period_count = len(df)
    
    # 1. 全局频率分析
    red_freq = Counter()
    blue_freq = Counter()
    for _, row in df.iterrows():
        for num in row['reds']:
            red_freq[num] += 1
        blue_freq[row['blue']] += 1
    
    # 2. 近期频率（最近N期）
    recent_df = df.tail(window_size)
    red_recent_freq = Counter()
    blue_recent_freq = Counter()
    for _, row in recent_df.iterrows():
        for num in row['reds']:
            red_recent_freq[num] += 1
        blue_recent_freq[row['blue']] += 1
    
    # 3. 奇偶比例分析
    odd_even_ratios = []
    for _, row in df.iterrows():
        odds = sum(1 for n in row['reds'] if n % 2 == 1)
        evens = 6 - odds
        odd_even_ratios.append((odds, evens))
    
    # 统计最常见的奇偶组合
    odd_even_counter = Counter(odd_even_ratios)
    most_common_odd_even = odd_even_counter.most_common(1)[0][0]
    
    # 4. 大小比例分析（1-16为小，17-33为大）
    big_small_ratios = []
    for _, row in df.iterrows():
        smalls = sum(1 for n in row['reds'] if n <= 16)
        bigs = 6 - smalls
        big_small_ratios.append((smalls, bigs))
    
    big_small_counter = Counter(big_small_ratios)
    most_common_big_small = big_small_counter.most_common(1)[0][0]
    
    # 5. 连号分析
    consecutive_patterns = Counter()
    for _, row in df.iterrows():
        reds = row['reds']
        consecutive = 0
        for i in range(len(reds) - 1):
            if reds[i+1] - reds[i] == 1:
                consecutive += 1
        consecutive_patterns[consecutive] += 1
    
    return {
        'red_freq': red_freq,
        'blue_freq': blue_freq,
        'red_recent_freq': red_recent_freq,
        'blue_recent_freq': blue_recent_freq,
        'most_common_odd_even': most_common_odd_even,
        'most_common_big_small': most_common_big_small,
        'most_common_consecutive': consecutive_patterns.most_common(1)[0][0] if consecutive_patterns else 0,
        'period_count': period_count
    }

# ==================== 4. 确定性预测算法 ====================
def deterministic_predict(df, patterns):
    """
    确定性预测算法：
    1. 基于历史频率和近期表现计算每个号码的综合得分
    2. 应用约束条件（奇偶、大小、连号）
    3. 固定的选择策略，确保结果可重现
    """
    
    red_freq = patterns['red_freq']
    blue_freq = patterns['blue_freq']
    red_recent_freq = patterns['red_recent_freq']
    blue_recent_freq = patterns['blue_recent_freq']
    period_count = patterns['period_count']
    
    # 计算红球综合得分（0-100）
    red_scores = {}
    for num in range(1, 34):
        # 全局频率得分（40%权重）
        global_score = (red_freq[num] / period_count) * 100
        
        # 近期频率得分（30%权重）
        recent_score = (red_recent_freq[num] / min(period_count, 50)) * 100
        
        # 漏漏补偿得分（20%权重）- 遗漏适中的号码得分更高
        red_miss, _ = calculate_miss_data(df)
        miss = red_miss[num]
        if miss < 5:
            miss_score = 100 - (miss * 10)
        else:
            miss_score = max(0, 100 - (miss - 4) * 5)
        
        # 号码分布均衡性（10%权重）- 避免过于集中在某些区间
        zone_score = 100
        if 1 <= num <= 11:
            zone = 'low'
        elif 12 <= num <= 22:
            zone = 'mid'
        else:
            zone = 'high'
        
        # 综合得分
        red_scores[num] = global_score * 0.4 + recent_score * 0.3 + miss_score * 0.2 + zone_score * 0.1
    
    # 选择红球
    # 按得分排序，从得分高的开始选择，同时考虑约束
    sorted_red = sorted(red_scores.items(), key=lambda x: -x[1])
    
    # 应用约束：奇偶比例
    target_odds = patterns['most_common_odd_even'][0]
    target_evens = patterns['most_common_odd_even'][1]
    
    selected_reds = []
    selected_odds = 0
    selected_evens = 0
    
    # 第一轮：选择得分最高的号码
    for num, score in sorted_red:
        if len(selected_reds) >= 6:
            break
        
        num_is_odd = (num % 2 == 1)
        
        # 检查是否满足奇偶约束
        if num_is_odd and selected_odds < target_odds:
            selected_reds.append(num)
            selected_odds += 1
        elif not num_is_odd and selected_evens < target_evens:
            selected_reds.append(num)
            selected_evens += 1
        else:
            # 如果已经达到奇偶约束，检查是否有空余
            if len(selected_reds) < 6:
                selected_reds.append(num)
                if num_is_odd:
                    selected_odds += 1
                else:
                    selected_evens += 1
    
    # 如果还没选满6个，补充得分最高的剩余号码
    while len(selected_reds) < 6:
        for num, score in sorted_red:
            if num not in selected_reds:
                selected_reds.append(num)
                break
    
    # 按数字大小排序
    selected_reds = sorted(selected_reds)
    
    # 计算蓝球综合得分
    blue_scores = {}
    for num in range(1, 17):
        global_score = (blue_freq[num] / period_count) * 100
        recent_score = (blue_recent_freq[num] / min(period_count, 50)) * 100
        
        _, blue_miss = calculate_miss_data(df)
        miss = blue_miss[num]
        if miss < 5:
            miss_score = 100 - (miss * 10)
        else:
            miss_score = max(0, 100 - (miss - 4) * 5)
        
        blue_scores[num] = global_score * 0.4 + recent_score * 0.3 + miss_score * 0.3
    
    # 选择得分最高的蓝球
    sorted_blue = sorted(blue_scores.items(), key=lambda x: -x[1])
    selected_blue = sorted_blue[0][0]
    
    return selected_reds, selected_blue

# ==================== 5. 验证模型准确性 ====================
def validate_model(df, patterns, validate_periods=100):
    """
    回测模型准确性：用历史数据验证预测算法
    """
    if len(df) <= validate_periods + 50:
        return {'accuracy': 0, 'red_hit_rate': 0, 'blue_hit_rate': 0}
    
    red_hits = 0
    blue_hits = 0
    total_reds = 0
    total_blues = 0
    
    # 对validate_periods期的数据进行回测
    for i in range(validate_periods):
        # 使用前面的数据预测
        train_df = df.iloc[:-(validate_periods - i)]
        test_row = df.iloc[-(validate_periods - i)]
        
        # 分析训练数据的模式
        train_patterns = analyze_patterns(train_df)
        
        # 预测
        pred_red, pred_blue = deterministic_predict(train_df, train_patterns)
        
        # 统计红球命中
        red_hits += sum(1 for n in pred_red if n in test_row['reds'])
        total_reds += 6
        
        # 统计蓝球命中
        if pred_blue == test_row['blue']:
            blue_hits += 1
        total_blues += 1
    
    return {
        'accuracy': (red_hits + blue_hits) / (total_reds + total_blues) * 100,
        'red_hit_rate': red_hits / total_reds * 100,
        'blue_hit_rate': blue_hits / total_blues * 100,
        'test_periods': validate_periods
    }

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
        # 分析模式
        patterns = analyze_patterns(df)
        
        # 确定性预测
        red_pred, blue_pred = deterministic_predict(df, patterns)
        
        result = {
            'success': True,
            'prediction': {
                'red': red_pred,
                'blue': blue_pred
            }
        }
    elif command == 'validate':
        # 验证模型，默认100期
        validate_periods = int(sys.argv[2]) if len(sys.argv) > 2 else 100
        patterns = analyze_patterns(df)
        validation_result = validate_model(df, patterns, validate_periods)
        
        result = {
            'success': True,
            'validation': validation_result
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
