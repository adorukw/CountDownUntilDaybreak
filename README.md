# CounDown: UntilDaybreak(倒数：直至破晓)

> 参加2026年GMTK game jam（之一）

## 一. 开发笔记

### 1. 解析tmj文件

#### 1.1 安装cJSON库

```bash
# 进项目根目录
cd .../CountDownUntilDaybreak

curl -L https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h -o include/cJSON.h
curl -L https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c -o src/cJSON.c
```

#### 1.2 安装cute_tiled.h
```bash
curl -L -o include/cute_tiled.h \
  https://raw.githubusercontent.com/RandyGaul/cute_headers/master/cute_tiled.h
```

#### 1.3 修复cute_tiled.h不支持的部分
cute_tiled v1.08 不认识对象的 opacity 字段。

你的 start.tmj 里每个对象都带有 "opacity":1，但 cute_tiled_object_t 结构体里没有 opacity 成员。cute_tiled 的解析器遇到未知 key 时不是跳过而是硬失败（CUTE_TILED_CHECK(0, "Unknown identifier found.") → goto cute_tiled_err），导致整个地图加载失败。

修复了什么
对 cute_tiled.h 做了两处微小修改（不破坏原作者代码风格）：

结构体 cute_tiled_object_t — 在 rotation 后加了 float opacity
解析 switch — 加了 case 11746902372727406098U: // opacity 分支，读取到 object->opacity
因为 opacity 的 FNV-1a 哈希值在图层解析里已经有（layer->opacity），所以只需要在对象解析的 switch 里加同样的 case 即可。不改任何业务逻辑，纯属补全 cute_tiled 对 Tiled 1.12 格式的兼容性。
