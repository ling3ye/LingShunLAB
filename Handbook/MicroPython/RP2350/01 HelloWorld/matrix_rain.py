import time
import random

# ==========================================
# Configuration Area
# 配置区域
# ==========================================

# Width of the matrix stream (number of columns)
# 瀑布流的宽度（列数），根据屏幕宽度调整
WIDTH = 40

# Refresh speed (seconds). Lower is faster.
# 刷新速度（秒）。数字越小，下落越快。
SPEED = 0.05

# ==========================================
# Colors & Assets
# 颜色与素材
# ==========================================

# ANSI escape codes for terminal colors
# 终端颜色代码 (ANSI转义序列)
GREEN = '\033[32m'          # Normal Green (Body) / 普通绿色 (雨滴身体)
BRIGHT_GREEN = '\033[1;32m' # Bright Green (Head) / 亮绿色 (雨滴头部)
RESET = '\033[0m'           # Reset Color / 重置颜色

# Character set: Uppercase letters and numbers
# 字符集：使用大写字母和数字，营造代码感
CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

# ==========================================
# State Initialization
# 状态初始化
# ==========================================

# Initialize the state of each column.
# List value represents remaining length of the rain drop.
# 初始化每一列的状态。
# 列表中的数值代表该列雨滴的“剩余长度”。
# 0 = Empty (空) | >0 = Raining (正在下雨)
columns = [0] * WIDTH

print(f"{GREEN}System Booting... / 系统启动中...{RESET}")
time.sleep(1)

# ==========================================
# Main Loop
# 主循环
# ==========================================

try:
    while True:
        # Initialize an empty string for the current line
        # 初始化一个空字符串，用于拼接当前这一行的内容
        line_content = ""
        
        # Iterate through every column (0 to WIDTH-1)
        # 遍历每一列 (从第 0 列 到 第 WIDTH-1 列)
        for i in range(WIDTH):
            
            # --------------------------------------
            # Case 1: The column is currently empty
            # 情况 1：这一列目前是空的
            # --------------------------------------
            if columns[i] == 0:
                # Randomly decide whether to start a new stream (2% chance)
                # 随机决定是否开始一串新雨滴 (2% 的概率)
                if random.random() > 0.98:
                    # Set a random length for this new stream (5-20 rows)
                    # 设定这串新雨滴的长度 (持续 5 到 20 行)
                    columns[i] = random.randint(5, 20)
                    
                    # Visual Effect: Use BRIGHT GREEN for the "Head"
                    # 视觉特效：雨滴的“头部”使用亮绿色
                    char = random.choice(CHARS)
                    line_content += f"{BRIGHT_GREEN}{char}{RESET} "
                else:
                    # No luck, stay empty. Add spaces to maintain alignment.
                    # 没运气，保持空白。添加空格以保持对齐。
                    line_content += "  "
            
            # --------------------------------------
            # Case 2: The column is active (raining)
            # 情况 2：这一列正在下雨
            # --------------------------------------
            else:
                # Decrease the remaining length counter
                # 雨滴剩余长度减 1
                columns[i] -= 1
                
                # Print the "Body" character in NORMAL GREEN
                # 显示雨滴的“身体”，使用普通绿色
                char = random.choice(CHARS)
                line_content += f"{GREEN}{char}{RESET} "

        # Print the fully assembled line to the console
        # 将拼装好的一整行打印到控制台
        print(line_content)
        
        # Control the speed
        # 控制刷新速度
        time.sleep(SPEED)

except KeyboardInterrupt:
    # Handle the Stop button cleanly
    # 优雅地处理停止按钮 (Ctrl+C)
    print(f"\n{RESET}Stream Stopped. / 程序已停止。")