# Emotion-recorder 🎯

一个基于 SDL3 的桌面情感记录工具，以四象限划分情绪维度（平淡/强烈 × 高兴/难过），实时显示当前时间，点击放置标记点，并支持每日限额、重置与数据持久化。

## ✨ 功能

- 📅 实时显示中文日期时间（每秒更新）
- 🧭 四个象限分别代表：平淡高兴、强烈高兴、平淡难过、强烈难过
- 🖱️ 点击象限放置绿色标记点，记录该时刻的情绪
- 📂 标记点自动保存至本地，重启后依然可见
- 📜 详细历史日志（每个点的时间 + 情绪象限）
  - 在save文件夹中dot_history里查看
- ⏱️ 每日限点模式（可选）：
  - 在setting.txt中控制，0为关闭，1为开启，默认为1
  - 启用后每天只能记录一次情绪
  - 「重置今日」按钮可撤销当天记录并立即重新记录
- 🧹 红色「清除存档」按钮（带确认对话框）
- 🖥️ F11 全屏 / ESC 退出全屏
- ⚙️ 首次运行自动生成默认配置与资源文件

## 📷 截图

<img src="Emotion recorder/ScreenShot1.png" width="600">
<img src="Emotion recorder/ScreenShot2.png" width="600">

## 🚀 快速开始

### 下载使用（推荐）

前往 [Releases](../../releases) 页面，下载最新版本的 `Emotion-recorder.zip`，解压后直接运行 `Emotion-recorder.exe`。

### 从源码编译

1. 克隆仓库
   ```bash
   git clone https://github.com/LiHua-25565/Emotion-recorder.git
