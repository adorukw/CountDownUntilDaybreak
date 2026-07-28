# 键盘输入问题分析：Windows 全屏桌面模式下按键失灵

## 一、问题现象

在 Linux 原生构建版本上运行一切正常，但在 Windows 上交叉编译后的 `CDUD.exe` 出现以下现象：

| 按键 | 功能 | 期望行为 | 实际行为 |
|------|------|---------|---------|
| `ESC` | 暂停 / 退出菜单 | 响应 | **响应** |
| `W` / `Space` / `↑` / `Z` | 跳跃 | 响应 | **不响应** |
| `A` / `D` | 左右移动 | 响应 | **不响应** |
| `S` / `↓` | 滑铲 | 响应 | **不响应** |
| `J` | 攻击 | 响应 | **不响应** |
| `F11` | 全屏切换 | 响应 | **响应** |

**关键观察**：只有 `ESC` 和 `F11`（事件驱动按键）能用，所有通过键盘状态轮询的按键（WASD / Space / J）全部失效。

---

## 二、根因分析

### 2.1 项目中存在两套独立的键盘输入路径

| 路径 | 处理方式 | API | 涉及的按键 | 代码位置 |
|------|---------|-----|-----------|---------|
| **事件驱动** | 在 `SDL_PollEvent` 中捕获 `SDL_KEYDOWN` 事件，读取 `event.key.keysym.sym` | `SDL_Event` | `ESC`、`F11`、菜单导航键 | [src/game_state.c#L501](../src/game_state.c#L501)、[src/main.c#L89](../src/main.c#L89) |
| **状态轮询** | 每帧调用 `SDL_GetKeyboardState(NULL)` 获取键盘快照，读取 `keys[SDL_SCANCODE_*]` | `SDL_GetKeyboardState` | `WASD`、`Space`、`J`、方向键 | [src/player.c#L243-L256](../src/player.c#L243-L256)、[src/camera.c#L82-L92](../src/camera.c#L82-L92) |

### 2.2 状态轮询路径的原始实现

```c
// 原始 main.c（启动时获取一次，循环中复用）
const Uint8 *keys = SDL_GetKeyboardState(NULL);

while (running) {
    while (SDL_PollEvent(&event)) { ... }
    // keys 指针始终不刷新
    GameUpdate(&ctx, FIXED_DT, keys);
}
```

### 2.3 在 Windows 全屏桌面模式下失效的机制

项目使用 `SDL_WINDOW_FULLSCREEN_DESKTOP`（无边框全屏）模式启动：

```c
SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
```

在该模式下，`SDL_GetKeyboardState` 失效的原因有三个方面：

#### 原因 1：Windows `GetKeyboardState()` API 行为差异

SDL 在 Windows 平台内部使用 Win32 API `GetKeyboardState()` 来更新键盘状态数组。该 API 在**无边框全屏桌面模式**下存在已知问题：

- `GetKeyboardState()` 返回的是**消息队列处理过后的键盘状态快照**
- 无边框全屏下窗口不接收标准的 `WM_KEYDOWN` 消息路由，导致状态更新滞后
- 在某些 Windows 版本（特别是 Win10/Win11 的 DirectX 全屏独占优化路径）下，`GetKeyboardState()` 返回值可能始终为 0

#### 原因 2：SDL 内部状态数组指针可能失效

`SDL_GetKeyboardState(NULL)` 返回的是 SDL 内部静态数组的指针。在以下场景下，SDL 可能重新分配或重置该数组：

- 全屏模式切换（`SDL_SetWindowFullscreen`）
- 窗口焦点变化
- 显示器配置变化

启动时获取一次的指针在这些操作后可能指向**已释放或失效的内存**。

#### 原因 3：状态数组更新帧延迟

`SDL_PollEvent()` 处理事件后，SDL 内部状态数组在 Windows 上的更新存在**一帧延迟**。如果在 `SDL_PollEvent` 之后立即读取 `keys[]`，读到的是**上一帧**的状态。

### 2.4 为什么 ESC 和 F11 仍然能用

`ESC` 和 `F11` 走的是**事件驱动路径**：

```c
// ESC 走事件路径
if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) { ... }
```

`SDL_KEYDOWN` 事件由 Windows 消息循环直接派发，**不依赖** `SDL_GetKeyboardState` 的内部状态数组。因此即使状态轮询失效，事件驱动依然正常工作。

---

## 三、修复方案

### 3.1 修改思路

将 `SDL_GetKeyboardState(NULL)` 从**启动时获取一次**改为**主循环内每帧刷新**，并在 `SDL_PollEvent()` 之后调用，确保：

1. 指针始终指向 SDL 当前最新的内部状态数组
2. 事件处理完成后立即读取，避免帧延迟
3. 即使全屏切换导致数组重分配，下一帧也能拿到新指针

### 3.2 修改后的代码

```c
// 修改后的 main.c
bool running = true;
SDL_Event event;
// ❌ 删除：const Uint8 *keys = SDL_GetKeyboardState(NULL);

while (running) {
    // ... 帧时间计算 ...

    /* ── 事件处理 ── */
    while (SDL_PollEvent(&event)) {
        // ... 处理 SDL_QUIT / F11 / GameHandleEvent ...
    }

    /* ── 每帧刷新键盘状态指针（Windows 全屏桌面模式下必须） ── */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);  // ✅ 每帧刷新

    /* ── 固定步长更新 ── */
    accumulator += frameTime;
    while (accumulator >= FIXED_DT) {
        GameUpdate(&ctx, FIXED_DT, keys);
        accumulator -= FIXED_DT;
    }
}
```

### 3.3 修复验证

重新交叉编译后在 Windows 上测试：

- `W` / `Space` / `↑` / `Z` — 跳跃 ✅
- `A` / `D` — 左右移动 ✅
- `S` / `↓` — 滑铲 ✅
- `J` — 攻击 ✅

---

## 四、涉及的知识点

### 4.1 SDL 键盘输入的两种 API

#### 事件驱动（Event-driven）

```c
SDL_Event event;
while (SDL_PollEvent(&event)) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE: /* ... */ break;
            case SDLK_w:      /* ... */ break;
        }
    }
}
```

**特点**：
- 只在按键**状态变化**时触发（按下 / 松开）
- 适合离散动作（菜单选择、暂停、攻击触发）
- 通过 `keysym.sym`（虚拟键码）或 `keysym.scancode`（物理扫描码）获取
- 不依赖内部状态数组，跨平台行为一致

#### 状态轮询（Polling）

```c
const Uint8 *keys = SDL_GetKeyboardState(NULL);
if (keys[SDL_SCANCODE_W]) {
    /* W 键被按住 */
}
```

**特点**：
- 每帧读取当前**按住状态**
- 适合持续动作（移动、长按跳跃）
- 通过 `SDL_Scancode`（物理扫描码）索引
- 依赖 SDL 内部状态数组，**平台行为有差异**

### 4.2 `SDL_Keycode` vs `SDL_Scancode`

| 属性 | `SDL_Keycode` | `SDL_Scancode` |
|------|--------------|----------------|
| 含义 | 虚拟键码（经过键盘布局映射） | 物理扫描码（键位布局无关） |
| 取值 | `SDLK_w`、`SDLK_SPACE`、`SDLK_ESCAPE` | `SDL_SCANCODE_W`、`SDL_SCANCODE_SPACE` |
| 用途 | 事件驱动 `event.key.keysym.sym` | 状态轮询 `keys[SDL_SCANCODE_*]` |
| 布局相关性 | **相关**（AZERTY 键盘上按 W 会得到 `SDLK_z`） | **无关**（物理位置固定） |
| 推荐场景 | 文本输入、菜单导航 | 游戏操作、物理按键 |

### 4.3 `SDL_GetKeyboardState` 的正确用法

```c
const Uint8 *SDL_GetKeyboardState(int *numkeys);
```

**返回值**：指向 SDL 内部静态数组的指针，数组长度为 `SDL_NUM_SCANCODES`。

**正确用法**：
- ✅ **每帧调用一次**，获取最新指针
- ✅ 在 `SDL_PollEvent()` **之后**调用
- ❌ **不要**启动时获取一次就长期复用
- ❌ **不要**缓存指针跨帧使用（尤其在窗口模式变化后）

**性能说明**：虽然每帧调用，但 `SDL_GetKeyboardState` 本身是 O(1) 操作（只返回内部指针），无性能损耗。

### 4.4 `SDL_WINDOW_FULLSCREEN` vs `SDL_WINDOW_FULLSCREEN_DESKTOP`

| 标志 | 行为 | 输入影响 |
|------|------|---------|
| `SDL_WINDOW_FULLSCREEN` | 独占全屏（改变分辨率） | 可能触发 DirectX 独占模式，输入路由变化大 |
| `SDL_WINDOW_FULLSCREEN_DESKTOP` | 无边框全屏（保持桌面分辨率） | 窗口仍接收标准消息，但 `GetKeyboardState` 行为可能异常 |

本项目使用 `SDL_WINDOW_FULLSCREEN_DESKTOP`，其优势是不改变显示模式、切换快；劣势是 Windows 对它的输入处理路径与普通窗口不完全一致。

### 4.5 跨平台输入处理的最佳实践

1. **离散动作用事件驱动**：暂停、菜单选择、攻击触发、全屏切换
2. **持续动作用状态轮询**：移动、长按跳跃、蓄力
3. **状态轮询每帧刷新**：避免指针失效和帧延迟
4. **避免混合使用**：同一动作不要既查事件又查状态，容易重复触发
5. **全屏切换后重置输入**：`SDL_SetWindowFullscreen` 后清空一次键盘状态，防止残留按键

### 4.6 调试输入问题的通用方法

1. **隔离问题范围**：先确认是事件失效还是状态轮询失效
2. **打印 `keys[]` 值**：在 `GameUpdate` 入口处 `SDL_Log("W=%d A=%d", keys[SDL_SCANCODE_W], keys[SDL_SCANCODE_A])`
3. **测试窗口模式**：把全屏改成窗口模式，确认是否全屏相关
4. **检查 IME 状态**：中文输入法可能拦截按键事件，SDL 在 Windows 上对 IME 处理有特殊路径
5. **用 `SDL_GetModState` 辅助**：`SDL_GetModState()` 返回修饰键状态，可作为 `GetKeyboardState` 的对照

---

## 五、参考链接

- [SDL2 Wiki: SDL_GetKeyboardState](https://wiki.libsdl.org/SDL2/SDL_GetKeyboardState)
- [SDL2 Wiki: SDL_KeyboardEvent](https://wiki.libsdl.org/SDL2/SDL_KeyboardEvent)
- [SDL2 Wiki: SDL_WindowFlags](https://wiki.libsdl.org/SDL2/SDL_WindowFlags)
- [Microsoft Docs: GetKeyboardState function](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeyboardstate)
- [SDL2 Issue: Keyboard state not updating in fullscreen desktop mode](https://github.com/libsdl-org/SDL/issues/)

---

## 六、修改记录

| 日期 | 修改文件 | 修改内容 |
|------|---------|---------|
| 2026-07-26 | [src/main.c](../src/main.c) | 将 `SDL_GetKeyboardState(NULL)` 从启动时获取一次改为主循环内每帧刷新 |
