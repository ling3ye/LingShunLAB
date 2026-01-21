// void setup() 函数在程序启动时运行一次
// The setup function runs once when you press reset or power the board
void setup() {
  // 初始化内置 LED 引脚为输出模式
  // Initialize the built-in LED pin as an output
  pinMode(LED_BUILTIN, OUTPUT);
}

// void loop() 函数会无限循环运行
// The loop function runs over and over again forever
void loop() {
  // 将引脚电压设为高电平，点亮 LED
  // Turn the LED on (HIGH is the voltage level)
  digitalWrite(LED_BUILTIN, HIGH); 
  
  // 等待 1000 毫秒（即 1 秒）
  // Wait for a second (1000 milliseconds)
  delay(1000); 
  
  // 将引脚电压设为低电平，熄灭 LED
  // Turn the LED off by making the voltage LOW
  digitalWrite(LED_BUILTIN, LOW); 
  
  // 再等待 1 秒，完成一个循环
  // Wait for a second
  delay(1000); 
}