# 豆绘 BeanDraw

面向手工 DIY 爱好者、布艺工作室、手作设计师的**多品类手作一体化设计平台**。

> **开源 · 免费 · 无广告** —— 本项目以 MIT 协议发布，任何人可自由使用、修改、分发。

- **架构**：Qt6 (C++) UI + Rust Core 静态库（FFI C ABI）
- **目标平台**：Windows（主开发平台，优先交付）、OpenHarmony（后期适配）
- **工程文件**：`.bean`（zip 容器 + manifest，跨平台互通）
- **开发大纲**：[`开发大纲.md`](开发大纲.md)

---

## ✨ 特性

- 十字绣：图像下采样 / KMeans 聚类 / DMC 色号匹配 / 符号网格 / 用量统计
- 纸样引擎：版型模板、缩放、缝份、DXF 导出
- 素材库：rusqlite 本地存储 + DMC 色卡
- 插件化模块框架（`ModulePlugin` trait + 注册表）
- 完整命令栈：撤销 / 重做（快照模式）
- Qt6 ADS 停靠系统 + i18n 国际化

---

## 📁 目录结构

```text
BeanDraw/
├── rust_core/      # Rust 跨平台业务内核（无 UI）：十字绣、纸样引擎、素材库、导出、工程文件
├── qt_ui/          # Qt6 C++ UI：主窗口 / ADS 停靠 / 画布 / 模块管理器 / i18n
├── build_scripts/  # Windows 构建打包 / 鸿蒙交叉编译 / 翻译更新脚本
├── resources/      # DMC 色卡 JSON、版型模板等运行时静态资源
├── assets/         # 文档与 README 所用静态图片（不打包进程序）
├── docs/           # 项目文档（中英文，按需新建）
├── output/         # 编译产物（gitignore）
├── LICENSE         # MIT 开源协议
└── README.md
```

---

## 🚀 快速开始（Windows）

**前置依赖**：Rust 1.98（`rust-toolchain.toml` 锁定）、**Qt 6.7+（推荐 6.12 LTS，Widgets API 稳定兼容；Qt 6.12 需 C++20，CMake 已配置）**、CMake 3.24+、MSVC。

> `RUN.bat` 会自动探测 `D:\QT` / `C:\Qt` 下的 Qt、CMake、Ninja 与 MSVC（vcvars64），无需手动配 PATH。

```powershell
# 一键构建（cargo → cmake → Qt）
powershell -File build_scripts/build_win.ps1

# 产物在 output/win/BeanDraw.exe
```

仅构建 / 测试内核：

```powershell
cd rust_core
cargo build
cargo test
cargo clippy --all-targets -- -D warnings
```

---

## 📊 当前进度（Phase 0 → Phase 1 交接点）

| 模块 | 状态 |
| --- | --- |
| FFI 层（句柄 / 数组传输 / 进度回调 / 错误码） | ✅ |
| 插件框架（ModulePlugin trait + 注册表） | ✅ |
| 命令栈（撤销 / 重做，快照模式） | ✅ |
| 十字绣（下采样 / KMeans / DMC 匹配 / 符号网格 / 统计） | ✅ |
| 导出（CSV / PNG / PDF / DXF） | ✅ |
| `.bean` 工程文件（zip + manifest + 预览图） | ✅ |
| 素材库（rusqlite + DMC 子集） | ✅ |
| Qt UI（主窗口 / ADS / 画布 / 工具 / Dock / i18n） | ✅ Qt 6.12 + MSVC 实测运行 |
| 冒烟测试（`BeanDraw.exe --smoke`，全链路无头验证） | ✅ |
| 鸿蒙 Qt 可行性 spike | ⏳ Phase 0 待办 |

---

## 🛠 开发规范速查

- Rust 内核只返回**数字错误码**，文案由 Qt 翻译；内核不含本地化字符串
- 批量数据 FFI 传输用「数组指针 + 长度」；Rust 内存仅 Rust 释放
- 所有工程状态变更必须经过命令栈（`ProjectDoc::pushHistory`）
- 新增第三方依赖须先在 `docs/license.md` 登记许可证（见开发大纲第 19 章）
- 提交规范：Conventional Commits；`Cargo.lock` 入库

---

## 📜 开源声明

BeanDraw 是**完全免费、开源**的软件，采用 **[MIT License](LICENSE)** 发布。

- ✅ 可免费用于个人、学习、商业场景
- ✅ 可自由复制、修改、合并、分发、再授权
- ✅ 可自由阅读源码、二次开发、发布衍生版本
- ⚠️ 作者不承担任何因使用本软件造成的直接或间接损失
- ⚠️ 分发时必须保留原始版权声明与 MIT 协议全文

本项目不内置任何付费功能、不强制登录、不收集用户数据。所有功能对所有人开放。

> 如果你在分发或二次开发本项目，请保留 `LICENSE` 文件与本声明，这是对开源社区最基本的尊重。
