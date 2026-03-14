---
name: ssq-lstm-predict
version: 1.1
author: LingShunLAB
description: 双色球 LSTM 预测 + 遗漏统计（读取你的3000多期CSV，自动算均值/遗漏/热冷 + LSTM预测下一期）
requires:
  bins: ["uv"]
---

# 🎯 双色球 LSTM 预测 + 遗漏统计 Skill

**一句话调用**：
- “用LSTM预测下一期双色球并输出当前所有号码遗漏次数”
- “双色球统计报告 + LSTM预测”
- “重新训练LSTM模型”

**功能**：
1. 读取你的开奖数据 CSV（支持你之前给的格式）
2. 计算每期红球均值 + 当前所有红球（1-33）+蓝球（1-16）的**遗漏期数**
3. 训练/加载 LSTM 模型（红球6位 + 蓝球1位）
4. 输出下一期预测号码 + 置信提示 + 统计报告

**数据路径**（默认）：
 {baseDir}/ssq.csv （请把你的CSV放这里，或修改代码里的路径）

**执行命令**（OpenClaw 自动调用，使用 uv 临时虚拟环境）：
```bash
uv run --with pandas --with numpy --with tensorflow --with tensorflow-metal --with scikit-learn python {baseDir}/lottery_lstm.py