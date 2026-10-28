# -*- coding: utf-8 -*-
"""
巴法云虚拟设备（PC 上位机模拟板子）
用途：没有开发板在身边时，用电脑模拟一块已联网的智能云家居，
      让微信小程序能正常显示实时数据、能点灯切换图标。

用法：
    python virtual_device.py               # 用脚本里写死的 UID
    python virtual_device.py <UID>          # 指定巴法云私钥 UID

前提：
    1. 该 UID 下已建好 devicectrl006（006 开关）和 sensordata004（004 传感器）两个 TCP 主题
    2. 主题名结尾的三位数字是设备类型码，缺了服务器会静默忽略（res=0）

行为：
    - 订阅 devicectrl006，收到 led:1 / beep:0 / rgb:2 / relay:1 就在终端打印并更新状态
    - 每 5 秒向 sensordata004 上报一帧 #温度#湿度#光照#烟雾#led#beep#rgb#relay#
    - 保持 TCP 长连接 = 小程序里"设备状态"显示"在线"

Ctrl+C 退出。
"""

import socket
import sys
import time

UID = "YOUR_BEMFA_UID"   # ← 换成你自己的巴法云私钥 UID
if len(sys.argv) >= 2:
    UID = sys.argv[1]

SERVER = "bemfa.com"
PORT = 8344
TOPIC_CTRL = "devicectrl006"
TOPIC_DATA = "sensordata004"

PUBLISH_PERIOD = 5.0

state = {"led": 0, "beep": 0, "rgb": 0, "relay": 0}
sensor = {"tem": 23.5, "hum": 55, "light": 3420, "smoke": 20}
SOCK = None


def send(sock, text):
    sock.sendall((text + "\r\n").encode("utf-8"))


def build_payload():
    return "#%s#%d#%d#%d#%d#%d#%d#%d#" % (
        str(sensor["tem"]),
        sensor["hum"],
        sensor["light"],
        sensor["smoke"],
        state["led"],
        state["beep"],
        state["rgb"],
        state["relay"],
    )


def handle_line(line):
    """解析服务器下发的一行，形如 cmd=2&uid=..&topic=devicectrl006&msg=led:1"""
    if "msg=" not in line:
        return
    msg = line.split("msg=", 1)[1].strip()
    if ":" not in msg:
        return
    key, val = msg.split(":", 1)
    key = key.strip()
    try:
        val = int(val.strip())
    except ValueError:
        return
    if key in state:
        state[key] = val
        print("[CLOUD] %-5s -> %d   当前: led=%d beep=%d rgb=%d relay=%d"
              % (key, val, state["led"], state["beep"], state["rgb"], state["relay"]))
        # 立刻回传一帧，小程序上的图标就不会被 3 秒轮询"打回去"
        if SOCK is not None:
            try:
                send(SOCK, "cmd=2&uid=%s&topic=%s&msg=%s"
                     % (UID, TOPIC_DATA, build_payload()))
            except Exception:
                pass


def main():
    global SOCK
    print("巴法云虚拟设备  UID=%s" % UID)
    sock = socket.create_connection((SERVER, PORT), timeout=10)
    SOCK = sock
    sock.setblocking(False)

    send(sock, "cmd=1&uid=%s&topic=%s" % (UID, TOPIC_CTRL))
    time.sleep(1.0)

    rx = b""
    connected = False
    try:
        chunk = sock.recv(4096)
        rx += chunk
    except Exception:
        pass
    if b"res=1" in rx:
        connected = True
        print("[OK] 订阅成功 %s (res=1)" % TOPIC_CTRL)
    else:
        print("[FAIL] 订阅应答: %r" % rx)
        print("       检查主题名是否以三位设备类型码结尾（006/004）")
        return

    # 首帧立即上报
    send(sock, "cmd=2&uid=%s&topic=%s&msg=%s" % (UID, TOPIC_DATA, build_payload()))
    print("[OK] 已上报首帧 %s 到 %s" % (build_payload(), TOPIC_DATA))

    last_pub = time.time()
    try:
        while True:
            now = time.time()
            if now - last_pub >= PUBLISH_PERIOD:
                last_pub = now
                # 让数值轻微变化，看起来像真的传感器
                sensor["tem"] = round(sensor["tem"] + 0.1, 1)
                if sensor["tem"] > 30.0:
                    sensor["tem"] = 22.0
                sensor["light"] = 3400 + int(now) % 40
                sensor["smoke"] = 18 + int(now) % 12
                send(sock, "cmd=2&uid=%s&topic=%s&msg=%s"
                     % (UID, TOPIC_DATA, build_payload()))
                print("[PUB ] %s" % build_payload())

            try:
                chunk = sock.recv(4096)
                if not chunk:
                    print("[WARN] 服务器断开，准备重连")
                    break
                rx += chunk
                while b"\r\n" in rx:
                    line, rx = rx.split(b"\r\n", 1)
                    handle_line(line.decode("utf-8", errors="replace"))
            except BlockingIOError:
                pass
            except Exception as exc:
                print("[WARN] 收包异常: %s" % exc)
                break

            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\n[EXIT] 用户中断")
    finally:
        try:
            sock.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
