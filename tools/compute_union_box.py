#!/usr/bin/env python3
"""
计算所有动画帧碰撞箱的并集 → 方案1（全局统一碰撞箱）

用法：python3 tools/compute_union_box.py

当你修改 frame_config.h 里的数值时，重新跑一次就行。
"""

# ── 从 frame_config.h 复制数据到这里 ──────────────────────────
PIVOT_X, PIVOT_Y = 27, 54

FRAMES = {
    "idle": [
        # offX  offY   w   h
        ( -21,  -45,  34, 41 ),
        ( -21,  -45,  34, 41 ),
        ( -21,  -45,  34, 41 ),
    ],
    "run": [
        (  -7,  -44,  23, 20 ),
        ( -15,  -43,  32, 39 ),
        ( -12,  -44,  26, 40 ),
        (  -9,  -45,  20, 42 ),
        (  -8,  -44,  22, 40 ),
        ( -15,  -43,  33, 39 ),
        ( -13,  -44,  27, 40 ),
        (  -8,  -45,  22, 41 ),
    ],
    "jump": [
        ( -21,  -52,  38, 45 ),
        ( -19,  -52,  39, 45 ),
        ( -21,  -53,  39, 45 ),
        ( -20,  -52,  39, 45 ),
    ],
    "fall": [
        ( -20,  -52,  39, 45 ),
    ],
}
# ─────────────────────────────────────────────────────────────

def compute_union(frames_dict, px, py):
    all_left, all_top   = [], []
    all_right, all_bottom = [], []

    for name, flist in frames_dict.items():
        for i, (ox, oy, w, h) in enumerate(flist):
            left   = px + ox
            top    = py + oy
            right  = left + w
            bottom = top + h
            all_left.append(left)
            all_top.append(top)
            all_right.append(right)
            all_bottom.append(bottom)

    min_l = min(all_left)
    min_t = min(all_top)
    max_r = max(all_right)
    max_b = max(all_bottom)

    print(f"并集碰撞箱（精灵图内像素范围）:")
    print(f"  left={min_l}, top={min_t}, right={max_r}, bottom={max_b}")
    print(f"  尺寸: {max_r - min_l} × {max_b - min_t}")
    print()
    print(f"相对 pivot({px},{py}) 的偏移:")
    print(f"  collisionOffX    = {min_l - px}")
    print(f"  collisionOffY    = {min_t - py}")
    print(f"  collisionWidth   = {max_r - min_l}")
    print(f"  collisionHeight  = {max_b - min_t}")
    print()
    # 翻转检查
    flip = -((min_l - px) + (max_r - min_l))
    print(f"翻转公式检查：flipOffX = -({min_l - px} + {max_r - min_l}) = {flip}")
    print(f"  朝左碰撞箱 left = pivot + ({flip}) = {px + flip}")
    print(f"  朝左碰撞箱 right = {px + flip + (max_r - min_l)}")
    print()

if __name__ == "__main__":
    compute_union(FRAMES, PIVOT_X, PIVOT_Y)
