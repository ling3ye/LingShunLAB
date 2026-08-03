// void setup() 在程序开始时运行一次，用于初始化
// The setup function runs once when you power the board or reset it
void setup() {
  // 将红、绿、蓝三个引脚设置为输出模式
  // Set the RGB pins as outputs
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  // 初始化状态：关闭所有颜色（注意：在共阳极电路中 HIGH 代表关闭）
  // Initialize all colors as OFF (Note: HIGH turns off the LED in Common Anode setup)
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

// void loop() 函数会不停地循环执行
// The loop function runs repeatedly in a cycle
void loop() {
  // --- 红色阶段 / Red Phase ---
  digitalWrite(LED_RED, LOW);   // 点亮红色 (Pull LOW to turn ON)
  delay(500);                   // 等待 0.5 秒 (Wait for 0.5 seconds)
  digitalWrite(LED_RED, HIGH);  // 熄灭红色 (Pull HIGH to turn OFF)

  // --- 绿色阶段 / Green Phase ---
  digitalWrite(LED_GREEN, LOW); // 点亮绿色
  delay(500);                   // 等待 0.5 秒
  digitalWrite(LED_GREEN, HIGH);// 熄灭绿色

  // --- 蓝色阶段 / Blue Phase ---
  digitalWrite(LED_BLUE, LOW);  // 点亮蓝色
  delay(500);                   // 等待 0.5 秒
  digitalWrite(LED_BLUE, HIGH); // 熄灭蓝色
}