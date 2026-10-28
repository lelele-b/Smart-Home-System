# 基于 STM32 的智能云家居系统

STM32F103ZET6 + 标准外设库（SPL V3.5.0）的智能家居工程，固件与微信小程序两端代码都在本仓库。功能：本地按键控制、TFT-LCD 实时显示、DHT11/光照/烟雾采集、SU03T 离线语音控制、ESP8266 + 巴法云上云、微信小程序远程监控。

---

## 一、功能一览

| 功能 | 实现方式 | 状态 |
| --- | --- | --- |
| LED 灯控制 | 4 路 LED（PE2~PE5），按键 KEY1 开关灯 1 | 已实测 |
| 继电器控制 | PA11 高电平吸合，按键 KEY2 切换 | 已实测 |
| RGB 灯 | PA8 红 / PA7 绿 / PA6 蓝，按键 KEY3 循环 关-红-绿-蓝 | 已实测 |
| 蜂鸣器 | PC0 高电平鸣叫，按键提示音 | 已实测 |
| 超限报警 | 烟雾 ≥ 120 或 温度 ≥ 35.0℃ 自动报警：蜂鸣器长鸣 + RGB 红灯 + 屏幕提示行 `!! SMOKE ALARM` + 云端 beep 位置 1 | 已实测（把阈值临时改小验证） |
| 屏幕显示 | 240×320 TFT（FSMC 并口），标题栏 + 4 个图标 + 数值 + 提示行 + 联网状态 | 已实测 |
| 温湿度 | DHT11 单总线（PG11），2 秒一次 | 已实测：54%RH / 23.2℃ |
| 光照 | LM393 模块（PA5 / ADC1_IN5）+ DMA | 已实测：约 3427 |
| 烟雾 | MQ2 模块（PC1 / ADC1_IN11）+ DMA | 已实测：约 20（洁净空气） |
| 语音控制 | SU03T 离线模块（UART5，9600） | 已验证串口框架，需人工说唤醒词确认 |
| 上云 | ESP8266（USART3，115200）+ 巴法云 TCP 透传 | ✅ 实测通过：云端能读数据、下发能控灯/RGB |
| 小程序 | 微信小程序（miniprogram 目录），含 RGB/继电器状态同步 | 已实测：数据能显示、能点灯 |

---

## 二、目录结构

```
SmartHome-STM32F103/
├─ project.uvprojx        Keil MDK5 工程（双击打开）
├─ User/                  main.c、中断向量表、外设库配置头
├─ System/                Delay.c/h（SysTick 1ms 节拍 + 微秒延时）
├─ Hardware/              所有外设驱动
│   ├─ LED / Beep / RGB / Relay / Key / Serial   基础外设
│   ├─ bsp_lcd.c/h、bsp_font.h、hz.h             厂商 LCD 驱动（勿改）
│   ├─ Ui.c/h                                    屏幕界面层
│   ├─ tem_1.h / hum_1.h / illu_1.h / mq2_1.h    4 个 80×80 RGB565 图标
│   ├─ led_1.h / beep_1.h / rgb_1.h / relay_1.h  备用图标（当前未用）
│   ├─ DHT.c/h                                   DHT11 单总线
│   ├─ ADC.c/h                                   光照 + 烟雾（ADC1+DMA）
│   ├─ Voice.c/h                                 SU03T 语音识别
│   └─ WiFi.c/h                                  ESP8266 + 巴法云
├─ Library/               ST 标准外设库 V3.5.0（23 个 .c）
├─ Start/                 启动文件、core_cm3、system_stm32f10x
├─ DebugConfig/           Keil 调试配置
├─ miniprogram/           微信小程序（用微信开发者工具打开）
└─ tools/                 配套小工具（virtual_device.py：没有开发板时在电脑上模拟设备）
```

代码风格与仓库内其它例程保持一致：4 空格缩进、`/* 中文注释 */`、函数前一行块注释、文件名首字母大写、`#ifndef __XXX_H` 头保护。**源码统一 GBK 编码**，用 Keil 打开显示正常。

---

## 三、引脚分配

| 外设 | 引脚 | 工作方式 |
| --- | --- | --- |
| LED1~LED4 | PE2 / PE3 / PE4 / PE5 | 推挽输出，低电平点亮 |
| 蜂鸣器 | PC0 | 推挽输出，高电平鸣叫 |
| RGB 灯 | PA8 红 / PA7 绿 / PA6 蓝 | 推挽输出，低电平点亮 |
| 继电器 | PA11 | 推挽输出，高电平吸合 |
| 按键 KEY1 | PA0 | 内部下拉输入，按下为高 |
| 按键 KEY2~KEY4 | PC4 / PC5 / PC6 | 内部上拉输入，按下为低 |
| DHT11 | PG11 | 单总线（输出推挽 / 输入浮空切换） |
| 光照 LM393 | PA5 | 模拟输入 ADC1_IN5 |
| 烟雾 MQ2 | PC1 | 模拟输入 ADC1_IN11 |
| TFT-LCD | FSMC：PD/PE7~PE15/PG0/PG12，背光 PB0 | 16 位并口，NE4 + A10 |
| 语音 SU03T | TX→PD2（UART5_RX），RX→PC12（UART5_TX） | 9600 8N1 |
| ESP8266 | TX→PB11（USART3_RX），RX→PB10（USART3_TX），RST→PE6 | 115200 8N1 |
| 调试串口 | PA9（TX）/ PA10（RX） | USART1，115200 8N1 |

---

## 四、软件结构

全部采用**时间片轮询**，不用阻塞式 delay 卡死主循环：

```c
while (1)
{
    now = Delay_GetTick();

    20ms   → Key_Scan()           按键扫描（连续 2 次一致才确认，40ms 消抖）
    50ms   → Cloud_Handle()       解析云端下发命令
    100ms  → Voice_Poll()         取语音命令
    500ms  → Ui_UpdateCloud()     刷新联网状态
    2000ms → Sensor_Update()      DHT11 + ADC（DHT11 不能读太快）
            └→ Alarm_Update()    判断是否超阈值，只在跳变时动作
    5000ms → WiFi_PublishData()   上传巴法云
    30s    → 断网重连
}
```

关键设计：

1. **`Device_SetAll()` 统一入口**：语音、按键、云端三条控制路径最后都调用它，保证本地状态变量和硬件实际状态永远一致。
2. **`Delay_us()` 用 SysTick->VAL**，不用 DWT：实测这块板子即使使能了 `DEMCR.TRCENA` 和 `DWT_CTRL.CYCCNTENA`，CYCCNT 也不递增（DHT11 读数会卡死）。`SysTick->VAL` 是 24 位向下计数器，1us 分辨率足够 DHT11。
3. **DHT11 读期间不能禁用 `NVIC_DisableIRQ(SysTick_IRQn)`**：SysTick 中断号是 -1，老版 CMSIS 的这个宏不做负数保护，会越界访问 NVIC 寄存器直接死机。正确做法是 `SysTick->CTRL &= ~(1UL << 1)` 只清 TICKINT 位。
4. **屏幕只重画变化的数字**：`Ui_UpdateData()` 逐位比较新旧字符串，避免整屏重绘造成闪烁。
5. **想在屏幕底部写自定义文字（可选扩展）**：厂商 `hz.h` 字库里只有 48 个固定汉字，装不下任意文字。做法是用取模软件（PCtoLCD2002，阴码/逐行式/高位在左）把缺的字生成 16×16 点阵共 32 字节存成头文件，再仿照 `Ui.c` 里的 `Ui_DrawText()` 写个小函数按位调用 `LCD_Fast_DrawPoint()` 逐点描出来；数字和字母直接用 `LCD_ShowString()`。注意 `LCD_Fill(x1,y1,x2,y2,c)` 的右下角坐标是**开区间**。
6. **超限报警带迟滞**：`Alarm_Update()` 里烟雾/温度各有一组"阈值 + 回差"（`ALARM_SMOKE_TH/HYST`、`ALARM_TEM_TH/HYST`，都在 `main.c` 顶部）。数值涨到阈值立刻报警，要掉到 `阈值 - 回差` 以下才解除，避免传感器在临界点抖动时蜂鸣器反复响。报警只在**状态跳变**那一刻动作，不会每 2 秒重复触发。

---

## 五、配置位置（三个占位符必须换成你自己的）

### 1. 单片机端 `Hardware/WiFi.h`

```c
#define WIFI_SSID      "YOUR_WIFI_NAME"        /* 手机热点（必须开 2.4G 频段） */
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"    /* 热点密码 */
#define WIFI_BEMFA_UID "YOUR_BEMFA_UID"        /* 巴法云私钥 UID（32 位十六进制） */
#define WIFI_TOPIC_SUB "devicectrl006"                      /* 订阅：下发控制 */
#define WIFI_TOPIC_PUB "sensordata004"                      /* 发布：上传数据 */
```

> ⚠️ **主题名必须以三位设备类型码结尾**（001 插座 / 002 灯泡 / 003 风扇 / 004 传感器 / 006 开关 …）。
> 不带类型码的主题（如 `devicectrl`、`sensordata`）在巴法云当前版本里会被**静默忽略**：
> 订阅回 `cmd=1&res=0`、发布回 `cmd=2&res=0`，云端 `online` 永远 false、`msg` 永远为空，
> 而程序端一句报错都没有 —— 这是本工程踩过的最大一个坑。
> 改成 `devicectrl006` / `sensordata004` 后立刻正常（`res=1`、`online=true`、`msg` 有值、下发能收到）。

> 热点必须是 **2.4G**：小米手机在「设置 → 个人热点 → 高级设置 → AP 频段」里选 2.4GHz，否则 ESP8266 扫不到。
> 仓库里这三个值都是 `YOUR_` 开头的占位符，**必须换成自己的**（热点名/密码、巴法云私钥 UID），否则连不上。
> 填好后的开机顺序是：连热点（`WIFI: join AP ok`）→ 连巴法云（`WIFI: bemfa ready`）→ 进入透传订阅控制主题。
> 没填也能直接烧：`WiFi_ConfigReady()` 检测到占位符就打印 `WIFI: config not ready, skip connect` 跳过联网，本地功能（按键/传感器/LCD/语音）照常运行，不会每次重连卡 15 秒。

### 2. 小程序端 `miniprogram/pages/index/index.js`

```js
uid: "YOUR_BEMFA_UID",                // 与单片机 WiFi.h 里的 UID 完全一致
ledtopic: "devicectrl006",                 // 控制主题
dhttopic: "sensordata004",                 // 数据主题
```

### 3. 巴法云控制台（账号已配好，供换账号时参考）

1. 注册 <https://cloud.bemfa.com>，控制台左侧复制「私钥」= UID（32 位十六进制）。
2. 新建两个 **TCP 协议**主题：`devicectrl006`（下发控制）、`sensordata004`（上传数据）。
   主题值只能是字母+数字，**结尾三位必须是设备类型码**（`006`=开关、`004`=传感器）。
3. 本工程用 **TCP 透传**（`AT+CIPMODE=1`），服务器 `bemfa.com:8344`。
4. 小程序还需要在「开发设置 → 服务器域名 → request 合法域名」里加 `https://api.bemfa.com` 和 `https://bemfa.com`。

---

## 六、编译与烧录

### 编译

双击 `project.uvprojx` → `Rebuild`。当前状态：**0 Error(s), 0 Warning(s)**，约 Code 30KB / RO 64KB。

### 烧录（板子上没有 ST-Link，用串口 ISP）

1. 板子 BOOT0 拨到 **1**，AI 复位或上电，进入串口 bootloader。
2. 用 FlyMcu（或任意支持 AN3155 的串口 ISP 工具）：
   - 芯片类型 `STM32F10x High-density`
   - 端口选 CH340（设备管理器里看，常见 COM5/COM7）
   - 波特率 115200，勾「校验」和「编程后执行」
3. 选 `Objects\SmartHome.hex` → 开始编程 → 烧完点「开始执行」，或把 BOOT0 拨回 **0** 再复位。

> BOOT0 保持在 1 时，复位后芯片停在 bootloader，程序不会自己跑，必须发 `Go 0x08000000`（FlyMcu 勾「编程后执行」就是做这件事）。

### 看串口日志

USART1 接 USB 转串口（PA9/PA10），串口助手 115200 8N1。正常开机输出：

```
LCD init start...
LCD init done.
===== Smart Home P5: CLOUD =====
SelfTest Done.
DHT OK  hum=54% tem=23.2C  LIGHT=3427 SMOKE=0019  err=0
```

---

## 七、通信协议

### 上传（单片机 → 巴法云 → 小程序）

```
cmd=2&uid=<UID>&topic=sensordata004&msg=#温度#湿度#光照#烟雾#led#beep#rgb#relay#
例：cmd=2&uid=xxx&topic=sensordata004&msg=#24.7#53#3429#46#0#0#0#0#
```

小程序按 `#` 分割，索引 1~8 依次是：温度、湿度、光照、烟雾、LED、蜂鸣器、RGB（0关/1红/2绿/3蓝）、继电器。

> 索引 6 的"蜂鸣器"位在报警期间会被强制上报为 `1`（即使本地手动关掉了蜂鸣器），小程序那一路"报警"图标就会亮起。

### 下发（小程序 → 巴法云 → 单片机）

```
cmd=2&uid=<UID>&topic=devicectrl006&msg=led:1
```

单片机解析：在收到的整帧里找 `led` / `beep` / `rgb` / `relay` 关键字，取冒号后面的数字，按位组合返回给主循环执行。

### 订阅

进入透传后立即发送：

```
cmd=1&uid=<UID>&topic=devicectrl006
```

---

## 八、实测记录

| 项目 | 结果 |
| --- | --- |
| 编译 | 0 Error(s), 0 Warning(s) |
| 烧录 | 串口 ISP 烧写 90KB，自动 Go 运行成功 |
| 自检 | 4 灯流水、RGB 三色、蜂鸣、继电器动作，串口输出 `SelfTest Done.` |
| DHT11 | 连续 10 次读数 checksum 全对：`hum=54% tem=23.2C err=0` |
| 光照 | ADC 稳定在 3426~3428（遮光/开灯会明显变化） |
| 烟雾 | 洁净空气 19~24，靠近酒精/打火机气体会上升 |
| 屏幕 | 标题栏、4 图标、数值每 2 秒刷新，按键/语音提示显示在提示行 |
| 上云 | `WIFI: join AP ok` → `WIFI: bemfa ready` → `WIFI connect ret=0`；云端 `getmsg` 读到 `#24.7#53#3429#46#0#0#0#0#` |
| 云控 | 云端下发 `led:1`、`rgb:2` 后串口回显 `CLOUD -> led=1 beep=0 rgb=2 relay=0`，灯与 RGB 同步变化，下一帧上报也带回新状态 |
| 超限报警 | 把 `ALARM_SMOKE_TH` 临时改成 5 后实测：串口打印 `ALARM -> SMOKE OVER LIMIT  smoke=63  th=5`，上报帧变成 `#24.4#56#3364#64#0#1#0#0#`（beep 位 = 1）；改回 120U 后不再误报，帧里 beep=0 |

---

## 九、排错速查

| 现象 | 原因 / 处理 |
| --- | --- |
| 烧完不运行 | BOOT0 还在 1，没执行跳转；拨回 0 或点「开始执行」 |
| 屏幕白屏/花屏 | FSMC 时序或背光 PB0；确认用的是本工程 `bsp_lcd.c`，别换他人版本 |
| DHT11 一直 `ERR` | 检查 PG11 接线与模块上拉电阻；`err=N` 连续增长说明没响应 |
| WiFi 连不上 | ① 热点不是 2.4G ② SSID/密码有空格 ③ UID 填错 ④ 热点没开 |
| 小程序显示离线 | 单片机没进透传（看串口有没有 `WIFI: bemfa ready`） |
| 云端 msg 一直是空 / online=false / res=0 | 主题名结尾缺三位设备类型码，见第五节说明 |
| 卡在 `WIFI: join AP ok` 之后 | 热点不是 2.4G，或手机热点已自动关闭 |
| 语音无反应 | 唤醒词要说「你好小智 / 小智小智」；确认模块 TX→PD2、RX→PC12、共地 |
| 蜂鸣器一直响 / 红灯常亮 | 说明真在报警态（或阈值设太小）：看串口 `ALARM -> ...`，改 `main.c` 顶部的 `ALARM_SMOKE_TH`、`ALARM_TEM_TH` |

---

## 十、没有开发板时怎么演示（虚拟设备）

`tools/virtual_device.py` 用电脑扮演一块已联网的板子：订阅 `devicectrl006`、每 5 秒向 `sensordata004` 上报一帧数据，
所以没有硬件时小程序照样能显示数据、能点灯（设备状态也会显示"在线"）。

```bash
pip install pyserial          # 只有烧录脚本需要；虚拟设备只用到标准库 socket
python tools/virtual_device.py <你的巴法云UID>
```

前提是该 UID 下已建好 `devicectrl006` / `sensordata004` 两个 TCP 主题。

---

## 十一、参考资料与致谢

- STM32F103 + 标准外设库驱动实践笔记（GPIO / LCD / 传感器 / SU03T 语音 / ESP8266+巴法云 / 微信小程序）
- 巴法云官方微信小程序示例（`miniprogram/` 目录即由此改来）
- ST 官方标准外设库 V3.5.0、ST7735S 数据手册、ESP8266 AT 指令集、巴法云开发文档 <https://cloud.bemfa.com/docs/>
- `Hardware/bsp_lcd.c`、`bsp_font.h`、`hz.h` 及 4 个图标头文件为厂商提供的驱动与图形资料，仅作学习用途
