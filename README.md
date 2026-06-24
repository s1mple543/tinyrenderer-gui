# tinyrenderer-gui 使用指南

> 一个基于 C++ 软件渲染器的 3D 模型查看器，带 GUI 界面。

---

## 一、从源码编译

### 环境要求

| 项目 | 要求 |
|------|------|
| 编译器 | MSVC 2022+ (Visual Studio 2022 Build Tools) |
| CMake | ≥ 3.20 |
| 系统 | Windows 10/11 |

首次构建会自动从 GitHub 下载 SDL2 和 Dear ImGui，无需手动安装。

### 编译步骤

**方式一：双击 build.bat**

```
tinyrenderer-gui/
  └─ build.bat        ← 双击运行即可
```

**方式二：命令行**

```bash
# 在项目根目录执行
cd tinyrenderer-gui

# 配置 (首次)
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl

# 编译
cmake --build build -j
```

> **注意**：以上命令需要在 **Visual Studio 开发命令提示符** (Developer Command Prompt) 中执行，
> 或者运行前先执行 `vcvarsall.bat x64` 设置环境变量。
>
> `build.bat` 已经处理了环境初始化，直接双击即可。

### 首次构建耗时

- SDL2 编译：约 1-2 分钟（仅首次）
- 项目自身编译：约 10 秒
- 后续增量编译：约 2-3 秒

---

## 二、运行

```bash
cd tinyrenderer-gui/build
./tinyrenderer-gui.exe
```

启动后界面：
- 主窗口：3D 渲染视口
- 左侧/浮动窗口：Controls 控制面板

---

## 三、模型导入

### 3.1 自动发现

项目启动时会自动扫描 `obj/` 目录下的所有 `.obj` 文件，无需手动注册。

### 3.2 目录结构要求

```
obj/
├── model_name/            ← 推荐：每个模型一个子目录
│   ├── model.obj
│   ├── model_diffuse.tga      ← 漫反射贴图 (可选)
│   ├── model_nm_tangent.tga   ← 法线贴图   (可选)
│   └── model_spec.tga         ← 高光贴图   (可选)
└── another_model.obj          ← 也支持直接放 obj/ 根目录
```

### 3.3 贴图命名规则

贴图必须与 `.obj` 文件**同名**，后缀遵循以下约定：

| 文件名 | 用途 | 格式要求 |
|--------|------|---------|
| `xxx_diffuse.tga` | 漫反射颜色贴图 | RGB (24bit) 或 RGBA (32bit) |
| `xxx_nm_tangent.tga` | 切线空间法线贴图 | RGB (24bit) |
| `xxx_spec.tga` | 高光强度贴图 | 灰度 (8bit) 或 RGB |

示例：
```
african_head.obj  → 自动查找:
                     african_head_diffuse.tga    ✓
                     african_head_nm_tangent.tga ✓
                     african_head_spec.tga       ✓
                     (不存在的贴图自动跳过)
```

> 贴图不是必须的。没有贴图时，着色器会使用默认颜色（白色），
> 仍可正常显示光照效果。

### 3.4 支持的 OBJ 格式

- **面格式**：支持三角形（`f v1 v2 v3`）和四边形（`f v1 v2 v3 v4`），四边形自动三角化
- **索引格式**：支持以下三种变体
  - `f v1 v2 v3` — 仅有位置
  - `f v1//n1 v2//n2 v3//n3` — 位置 + 法线
  - `f v1/t1/n1 v2/t2/n2 v3/t3/n3` — 位置 + 纹理 + 法线
- **文件大小**：建议三角形数 ≤ 10000，否则首次加载和旋转会有明显卡顿
- **编码**：文件名建议用英文，避免编码问题

### 3.5 快速开始

把你自己的 `.obj` 文件连同 `.tga` 贴图放入 `obj/` 目录：

```bash
obj/
  └── my_model/
      ├── my_model.obj
      ├── my_model_diffuse.tga
      └── my_model_spec.tga
```

重新启动程序，在 GUI 的模型列表中勾选即可显示。

> 不需要重新编译！只需要把文件放进 `obj/` 目录。

---

## 四、GUI 操作说明

### 模型控制

| 操作 | 说明 |
|------|------|
| **复选框** | 勾选/取消显示单个模型 |
| **All On / All Off** | 一键显示/隐藏所有模型 |
| 首次勾选 | 自动加载模型（延迟加载，启动时不加载任何模型） |

### 视角控制

| 操作 | 说明 |
|------|------|
| **左键拖拽** | 旋转视角（围绕场景中心） |

### 着色器

| 着色器 | 说明 |
|--------|------|
| Phong (Bump+Spec) | 完整光照：漫反射 + 法线贴图 + 高光贴图 |
| Flat | 纯色平直着色 |
| Normal Visualize | 法线方向可视化 |
| Texture Only | 仅漫反射贴图，无光照 |
| Depth | 深度可视化 |

### 其他设置

- **Light Dir** — 光源方向 (x, y, z)
- **Ambient** — 环境光强度
- **FOV** — 视野角度 (默认 90°)
- **Textures** — 漫反射贴图开关
- **Normal Map** — 法线贴图开关
- **Wireframe** — 三角形线框叠加
- **Reset Camera** — 相机重新居中到所有可见模型

---

## 五、常见问题

### Q: 启动后窗口是黑的 / 没有画面

启动时所有模型默认隐藏。在 Controls 窗口中勾选一个模型即可显示。

### Q: 加载模型报 "Failed to open"

检查 `obj/` 目录是否存在以及文件路径是否正确。程序启动时会在终端打印扫描到的模型列表。

### Q: 模型渲染全黑

可能原因：
1. 光源方向不对 — 调整 Light Dir
2. 法线贴图缺失 — 关闭 Normal Map 试试
3. 模型法线数据异常 — 切换到 Flat 着色器验证

### Q: 渲染很卡

- 模型三角形过多（建议 ≤ 10000）
- 不要同时勾选过多大模型
- 尝试切换到 Texture Only 或 Flat 着色器（减少计算量）

### Q: 如何转换自己的模型?

1. 在建模软件（Blender、3ds Max 等）中导出为 OBJ 格式
2. 确保勾选"三角化"(Triangulate) 选项
3. 导出贴图为 TGA 格式（或用 Photoshop/GIMP 转换）
4. 按命名规则放入 `obj/` 目录

---

## 六、编译故障排除

| 错误 | 原因 | 解决 |
|------|------|------|
| `CMAKE_CXX_COMPILER not found` | 找不到 MSVC | 运行 `vcvarsall.bat x64` 或在 VS 开发命令提示符中执行 |
| `LNK1168: cannot open for writing` | 程序正在运行 | 任务管理器结束 `tinyrenderer-gui.exe` |
| FetchContent 下载失败 | 网络问题 | 重试，或配置代理 |
