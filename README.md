# 📓 记事簿 — 液态玻璃风格笔记本 (C++ Qt6)

Apple 液态玻璃设计风格的桌面记事本，年轻简约，专注每日笔记与定时提醒。C++ Qt6 原生开发，编译为独立 exe，无需 Python 运行时。

## ✨ 功能

- **笔记管理** — 创建、编辑、搜索笔记，支持分类和优先级标记，本地 JSON 文件存储
- **定时提醒** — 设置日期时间提醒，支持重复，到时间自动通知
- **窗口置顶** — Win32 SetWindowPos 实现，一键钉在所有应用前面
- **开机自启** — Windows 注册表管理
- **全局热键** — `Ctrl+Shift+N` 任意应用中弹出/隐藏
- **系统托盘** — 关闭时最小化到托盘
- **4 套主题** — 白色磨砂 / 暗黑磨砂 / 暖米色 / 极简白
- **HTTP API** — 内置 REST 接口 (localhost:18520)，供 AI Agent 测试调用
- **窗口记忆** — 自动记住位置和大小

## ⌨️ 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+N` | 新建笔记 |
| `Ctrl+S` | 保存 |
| `Ctrl+F` | 搜索 |
| `Ctrl+W` | 关闭编辑器 |
| `Ctrl+Q` | 退出 |
| `Ctrl+1/2/3` | 切换标签页 |
| `Ctrl+Shift+N` | 全局热键 — 弹出/隐藏 |
| `Esc` | 关闭弹窗 |

## 🤖 HTTP API (AI Agent 测试接口)

应用启动后自动在 `http://127.0.0.1:18520` 提供 REST API：

```bash
# 健康检查
curl http://127.0.0.1:18520/api/health

# 笔记 CRUD
curl http://127.0.0.1:18520/api/notes
curl -X POST http://127.0.0.1:18520/api/notes -H "Content-Type: application/json" -d '{"title":"标题","content":"内容","priority":2,"category":"工作"}'
curl -X PUT http://127.0.0.1:18520/api/notes/1 -H "Content-Type: application/json" -d '{"title":"新标题"}'
curl -X DELETE http://127.0.0.1:18520/api/notes/1

# 提醒
curl http://127.0.0.1:18520/api/reminders
curl -X POST http://127.0.0.1:18520/api/reminders -H "Content-Type: application/json" -d '{"title":"会议","trigger_at":"2026-06-11 15:30:00","is_repeated":false,"repeat_interval_min":0}'

# 设置
curl http://127.0.0.1:18520/api/settings/theme
curl -X PUT http://127.0.0.1:18520/api/settings/theme -H "Content-Type: application/json" -d '{"value":"dark"}'

# 窗口控制
curl -X POST http://127.0.0.1:18520/api/bridge/minimize
curl -X POST http://127.0.0.1:18520/api/bridge/toggle-on-top
```

## 🛠 技术栈

| 层 | 技术 |
|----|------|
| 窗口 | Qt6 QMainWindow — 无边框、Win32 拖动、DWM 圆角 |
| 渲染 | QWebEngineView (Chromium) |
| 桥接 | QWebChannel — JS ↔ C++ 通信 |
| HTTP | QTcpServer — 内嵌 REST API |
| 存储 | 本地 JSON 文件 (`%APPDATA%/Notebook/`) |
| 定时 | QTimer + QThread — 30 秒轮询提醒 |
| 构建 | CMake + MSVC 2022 |

## 🚀 编译

**环境**：Windows 10/11，MSVC 2022，CMake 4.2+

```bash
# 安装 Qt6 SDK
pip install aqtinstall
aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtwebengine qtwebchannel qtpositioning --outputdir C:/Qt

# 克隆
git clone https://github.com/csj521/notebook.git
cd notebook

# 编译
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release

# 打包 (复制 Qt DLL)
C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe build/Release/Notebook.exe --no-translations
```

## 📁 项目结构

```
notebook/
├── CMakeLists.txt          # CMake 构建
├── resources.qrc           # Qt 资源文件
├── assets/                 # 前端 (HTML/CSS/JS)
│   ├── index.html
│   ├── style.css           # 4 套主题
│   └── script.js
└── src/
    ├── main.cpp            # 入口
    ├── MainWindow.h/cpp    # 无边框窗口 + 拖动 + 托盘
    ├── BridgeApi.h/cpp     # QWebChannel API (JS ↔ C++)
    ├── Storage.h/cpp       # JSON 文件存储
    ├── Scheduler.h/cpp     # 定时提醒引擎
    ├── AutoStart.h/cpp     # 注册表开机自启
    ├── ApiServer.h/cpp     # HTTP REST API 服务器
    ├── Note.h              # 笔记数据模型
    └── Reminder.h          # 提醒数据模型
```

## 📄 License

MIT
