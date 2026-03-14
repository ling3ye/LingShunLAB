import pandas as pd
import numpy as np
import os
import joblib
import tensorflow as tf
from tensorflow.keras.models import Sequential, load_model
from tensorflow.keras.layers import LSTM, Dense, Dropout
from tensorflow.keras.optimizers import Adam
from sklearn.preprocessing import MinMaxScaler
import warnings

warnings.filterwarnings('ignore')

# ==================== 配置区 ====================
CSV_PATH = os.path.expanduser("~/openclaw/workspace/skills/ssq-lstm-predict/ssq.csv")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_PATH = os.path.join(SCRIPT_DIR, "ssq_lstm_model.keras")
SCALER_PATH = os.path.join(SCRIPT_DIR, "scaler.joblib")

PAST_STEPS = 20
EPOCHS = 50
BATCH_SIZE = 32

# ==================== 1. 读取 + 强制最旧→最新 ====================
def load_data():
    if not os.path.exists(CSV_PATH):
        raise FileNotFoundError(f"❌ 未找到CSV: {CSV_PATH}")
    
    df = pd.read_csv(CSV_PATH)
    red_cols = ['ball1', 'ball2', 'ball3', 'ball4', 'ball5', 'ball6']
    df['reds'] = df[red_cols].values.tolist()
    df['reds'] = df['reds'].apply(lambda x: sorted([int(n) for n in x]))
    df['blue'] = df['special_ball'].astype(int)
    
    df = df.sort_values(by='issue_no', ascending=True).reset_index(drop=True)
    
    print(f"✅ 数据加载成功！共 {len(df)} 期（已排序最旧→最新）")
    return df

# ==================== 2. 遗漏统计 ====================
def calculate_stats(df):
    period_count = len(df)
    latest = df.iloc[-1]
    
    red_miss = {}
    for num in range(1, 34):
        last_appear = next((period_count - 1 - i for i in range(period_count-1, -1, -1) if num in df.iloc[i]['reds']), period_count)
        red_miss[num] = last_appear
    
    blue_miss = {}
    for num in range(1, 17):
        last_appear = next((period_count - 1 - i for i in range(period_count-1, -1, -1) if df.iloc[i]['blue'] == num), period_count)
        blue_miss[num] = last_appear
    
    latest_mean = np.mean(latest['reds'])
    
    report = f"""
📊 当前统计报告（共 {period_count} 期）
最新一期红球均值：{latest_mean:.2f}
红球最大遗漏：{max(red_miss.values())} 期（号码 {max(red_miss, key=red_miss.get):02d}）
蓝球最大遗漏：{max(blue_miss.values())} 期（号码 {max(blue_miss, key=blue_miss.get):02d}）
🔥 热号（遗漏0期）：{[k for k, v in red_miss.items() if v == 0]}
❄️ 冷号（遗漏≥10期）：{[k for k, v in red_miss.items() if v >= 10]}
"""
    return report, red_miss, blue_miss

# ==================== 3. LSTM 模型 ====================
def build_model():
    model = Sequential([
        LSTM(64, return_sequences=True, input_shape=(PAST_STEPS, 7)),
        Dropout(0.2),
        LSTM(32),
        Dropout(0.2),
        Dense(7)
    ])
    model.compile(optimizer=Adam(learning_rate=0.001), loss='mse')
    return model

def prepare_sequences(df):
    data = np.array([row['reds'] + [row['blue']] for _, row in df.iterrows()])
    scaler = MinMaxScaler()
    data_scaled = scaler.fit_transform(data)
    X, y = [], []
    for i in range(PAST_STEPS, len(data_scaled)):
        X.append(data_scaled[i - PAST_STEPS:i])
        y.append(data_scaled[i])
    return np.array(X), np.array(y), scaler

def train_or_load_model(df):
    gpus = tf.config.list_physical_devices('GPU')
    if gpus:
        print(f"✅ Metal GPU 加速已启用！")
    
    if os.path.exists(MODEL_PATH) and os.path.exists(SCALER_PATH):
        print("✅ 加载已训练模型...")
        model = load_model(MODEL_PATH)
        scaler = joblib.load(SCALER_PATH)
    else:
        print("首次训练 LSTM（Mac GPU 加速中）...")
        X, y, scaler = prepare_sequences(df)
        model = build_model()
        model.fit(X, y, epochs=EPOCHS, batch_size=BATCH_SIZE, validation_split=0.1, verbose=1)
        model.save(MODEL_PATH)
        joblib.dump(scaler, SCALER_PATH)
    return model, scaler

# ==================== 4. 预测下一期 ====================
def predict_next(model, df, scaler):
    recent = np.array([row['reds'] + [row['blue']] for _, row in df.tail(PAST_STEPS).iterrows()])
    recent_scaled = scaler.transform(recent).reshape(1, PAST_STEPS, 7)
    pred_scaled = model.predict(recent_scaled, verbose=0)[0]
    pred = scaler.inverse_transform([pred_scaled])[0]
    
    temp_red = [int(round(x)) for x in pred[:6]]
    red_pred = sorted(set(max(1, min(33, x)) for x in temp_red))
    if len(red_pred) < 6:
        used = set(red_pred)
        for n in range(1, 34):
            if n not in used and len(red_pred) < 6:
                red_pred.append(n)
    red_pred = sorted(red_pred[:6])
    blue_pred = max(1, min(16, int(round(pred[6]))))
    return red_pred, blue_pred

# ==================== 新增：历史回测（重点报告平均表现） ====================
def walk_forward_backtest(model, df, scaler):
    print("\n🔍 正在进行历史前向回测（评估模型平均表现）...")
    red_matches = []
    blue_matches = []
    total_matches = []
    high_match_count = 0  # 例如匹配 ≥ 4 的次数
    
    for i in range(PAST_STEPS, len(df)):
        recent_df = df.iloc[i - PAST_STEPS : i]
        recent = np.array([row['reds'] + [row['blue']] for _, row in recent_df.iterrows()])
        recent_scaled = scaler.transform(recent).reshape(1, PAST_STEPS, 7)
        
        pred_scaled = model.predict(recent_scaled, verbose=0)[0]
        pred = scaler.inverse_transform([pred_scaled])[0]
        
        temp_red = [int(round(x)) for x in pred[:6]]
        red_pred = sorted(set(max(1, min(33, x)) for x in temp_red))
        if len(red_pred) < 6:
            used = set(red_pred)
            for n in range(1, 34):
                if n not in used and len(red_pred) < 6:
                    red_pred.append(n)
        red_pred = sorted(red_pred[:6])
        blue_pred = max(1, min(16, int(round(pred[6]))))
        
        actual_red_set = set(df.iloc[i]['reds'])
        actual_blue = df.iloc[i]['blue']
        
        red_hit = len(set(red_pred) & actual_red_set)
        blue_hit = 1 if blue_pred == actual_blue else 0
        total_hit = red_hit + blue_hit
        
        red_matches.append(red_hit)
        blue_matches.append(blue_hit)
        total_matches.append(total_hit)
        
        if total_hit >= 4:
            high_match_count += 1
    
    # 计算统计
    avg_red = np.mean(red_matches)
    avg_blue = np.mean(blue_matches) * 100  # 百分比
    avg_total = np.mean(total_matches)
    max_total = max(total_matches)
    min_total = min(total_matches)
    high_ratio = (high_match_count / len(total_matches)) * 100 if total_matches else 0
    
    print(f"""
回测结果统计（基于 {len(total_matches)} 次历史预测）：
平均红球命中：{avg_red:.2f} 个
平均蓝球命中率：{avg_blue:.1f} %
平均总命中：{avg_total:.2f} 个
单次最高命中：{max_total} 个
单次最低命中：{min_total} 个
命中 ≥ 4 个的占比：{high_ratio:.1f} %
""")
    
    if avg_total < 1.0:
        print("⚠️ 平均总命中偏低，模型当前表现一般（可能需要更多数据/调参）")
    elif avg_total >= 2.0:
        print("👍 平均表现不错，模型有一定捕捉能力")
    else:
        print("模型平均表现中等，继续观察或优化参数")
    
    return avg_total

# ==================== 主函数（替换原版） ====================
def main():
    df = load_data()
    stats_report, _, _ = calculate_stats(df)
    model, scaler = train_or_load_model(df)
    
    # 重点：历史回测，输出平均表现
    avg_performance = walk_forward_backtest(model, df, scaler)
    
    # 预测真正下一期
    red_pred, blue_pred = predict_next(model, df, scaler)
    
    print("\n" + stats_report)
    print(f"🚀 LSTM预测**下一期**：红球 {red_pred} + 蓝球 {blue_pred:02d}")
    print(f"（模型历史平均总命中：{avg_performance:.2f} 个）")
    print("⚠️ 提醒：彩票纯随机，此预测仅供学习娱乐参考！")