import pandas as pd
import numpy as np
import os
import sys
import json
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
        return None
    
    df = pd.read_csv(CSV_PATH)
    red_cols = ['ball1', 'ball2', 'ball3', 'ball4', 'ball5', 'ball6']
    df['reds'] = df[red_cols].values.tolist()
    df['reds'] = df['reds'].apply(lambda x: sorted([int(n) for n in x]))
    df['blue'] = df['special_ball'].astype(int)
    
    df = df.sort_values(by='issue_no', ascending=True).reset_index(drop=True)
    return df

# ==================== 2. 遗漏统计 ====================
def calculate_miss_data(df):
    if df is None:
        return {}, {}
    
    period_count = len(df)
    
    red_miss = {}
    for num in range(1, 34):
        last_appear = next((period_count - 1 - i for i in range(period_count-1, -1, -1) if num in df.iloc[i]['reds']), period_count)
        red_miss[num] = last_appear
    
    blue_miss = {}
    for num in range(1, 17):
        last_appear = next((period_count - 1 - i for i in range(period_count-1, -1, -1) if df.iloc[i]['blue'] == num), period_count)
        blue_miss[num] = last_appear
    
    return red_miss, blue_miss

def calculate_stats(df):
    if df is None:
        return None
    
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
    if df is None:
        return None, None
    
    if os.path.exists(MODEL_PATH) and os.path.exists(SCALER_PATH):
        model = load_model(MODEL_PATH)
        scaler = joblib.load(SCALER_PATH)
    else:
        X, y, scaler = prepare_sequences(df)
        model = build_model()
        model.fit(X, y, epochs=EPOCHS, batch_size=BATCH_SIZE, validation_split=0.1, verbose=0)
        model.save(MODEL_PATH)
        joblib.dump(scaler, SCALER_PATH)
    return model, scaler

def predict_next(model, df, scaler):
    if model is None or df is None:
        return None, None
    
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

# ==================== API 接口 ====================
def main():
    command = sys.argv[1] if len(sys.argv) > 1 else 'stats'
    
    df = load_data()
    
    if command == 'miss':
        red_miss, blue_miss = calculate_miss_data(df)
        result = {
            'success': True,
            'redMiss': red_miss,
            'blueMiss': blue_miss
        }
    elif command == 'predict':
        model, scaler = train_or_load_model(df)
        red_pred, blue_pred = predict_next(model, df, scaler)
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
