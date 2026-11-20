#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
epd_photo.py —— 把任意彩色照片转成 OSPTEK 3.7" E6（480x720 / JD7601）可显示的帧数据

这块屏只有 6 种颜色（黑 / 白 / 黄 / 红 / 蓝 / 绿）、没有灰阶，
所以「照片」不可能直接量化，必须：色调映射 -> OKLab 里抖动 -> 打包成 172800 字节。

处理链路（顺序有意义，每一步都在补前一步的坑）：

    sRGB 照片
      ├─ 裁切 / 缩放到 480x720（Lanczos）
      ├─ 曝光（线性域）            --exposure
      ├─ 转 OKLab（感知均匀空间）
      ├─ 对比 / 伽马 / 暗部 / 亮部  --contrast --gamma --shadows --highlights
      ├─ 饱和                        --saturation
      ├─ L 范围压缩到面板实际动态范围（白不是 255，黑不是 0）
      ├─ 调色板匹配 + 抖动           --method nearest|bayer|fs|stucki|atkinson|dbs
      └─ 打包成列优先 + Y 镜像 + 每字节 2 像素的 172800 字节

用法示例：

    python3 epd_photo.py photo.jpg --header photo_data.h   # 生成可直接编进固件的数组
    python3 epd_photo.py photo.jpg -o photo.bin --preview preview.png
    python3 epd_photo.py photo.jpg --variants out/          # 一次出 4 种算法对比
    python3 epd_photo.py photo.jpg --header data.h --header-name my_photo --guarded

依赖：pillow + numpy（pip install pillow numpy）。
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps

# --------------------------------------------------------------------------- #
# 面板常量（与 docs/03-硬件与驱动参考.md 第 4 节一致）
# --------------------------------------------------------------------------- #

W, H = 480, 720          # 竖屏分辨率
COL_BYTES = H // 2       # 每条竖列 360 字节
FRAME_BYTES = W * COL_BYTES  # 172800

#: 4bit 色值。注意 0x4 是非法值，不是 0~5 连续编号。
CODE = {
    "black": 0x0,
    "white": 0x1,
    "yellow": 0x2,
    "red": 0x3,
    "blue": 0x5,
    "green": 0x6,
}
NAMES = ["black", "white", "yellow", "red", "blue", "green"]

# --------------------------------------------------------------------------- #
# 调色板
# --------------------------------------------------------------------------- #
#
# 关键概念：**设备色值 ≠ 肉眼看到的颜色**。
#
#   * 面板内部用的是「满饱和」的色值（红就是 #FF0000），但电子纸靠反射环境光成色，
#     颜料本身吸收掉一部分光，实测观感是深砖红、藏青、墨绿，白也只有 ~#DEDED8。
#   * 抖动（dithering）的数学本质是「用两个颜色的空间混色去逼近第三种颜色」，
#     混出来的颜色由**观感**决定，而不是由驱动里的 4bit 码决定。
#     所以：**按观感色算距离，按设备码输出**。这两件事必须分开。
#
#   * `typical` 是按 E6 / Spectra 6 类面板的典型观感估的初值（无光度计可用）。
#     有条件的量一下自己那块屏：刷 6 张纯色，用相机/取色器读数，写进 --palette 的 json。
#   * `nominal` 是「不看观感、直接拿 0/255 当颜色」的常见做法，留作对照用
#     —— 你会发现它虽然也能出图，但整体发灰、色相偏。
#
PALETTES = {
    "typical": {
        "black": (22, 22, 24),
        "white": (226, 226, 219),
        "yellow": (232, 194, 22),
        "red": (168, 30, 40),
        "blue": (30, 62, 138),
        "green": (36, 108, 62),
    },
    "nominal": {
        "black": (0, 0, 0),
        "white": (255, 255, 255),
        "yellow": (255, 255, 0),
        "red": (255, 0, 0),
        "blue": (0, 0, 255),
        "green": (0, 255, 0),
    },
}


# --------------------------------------------------------------------------- #
# 色彩空间：sRGB <-> 线性 <-> OKLab
# --------------------------------------------------------------------------- #
#
# 为什么是 OKLab 而不是 RGB / Lab / LCh：
#   * RGB 欧氏距离不符合人眼 —— 深蓝和黑只差 30 个单位，黄和白差 60 单位，
#     量化时会大面积把蓝色判成黑色。
#   * OKLab 是感知均匀的，且 hue 是直的（LCh 系在低饱和处有「被灰吸走」的坑）。
#   * 计算量和 RGB 一样便宜，不需要查表。


def srgb_to_linear(x: np.ndarray) -> np.ndarray:
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1 / 2.4) - 0.055)


_M1 = np.array(
    [
        [0.4122214708, 0.5363325363, 0.0514459929],
        [0.2119034982, 0.6806995451, 0.1073969566],
        [0.0883024619, 0.2817188376, 0.6299787005],
    ]
)
_M2 = np.array(
    [
        [0.2104542553, 0.7936177850, -0.0040720468],
        [1.9779984951, -2.4285922050, 0.4505937099],
        [0.0259040371, 0.7827717662, -0.8086757660],
    ]
)
_M1_INV = np.linalg.inv(_M1)
_M2_INV = np.linalg.inv(_M2)


def linear_to_oklab(lin: np.ndarray) -> np.ndarray:
    lms = lin @ _M1.T
    lms = np.cbrt(np.maximum(lms, 0.0))
    return lms @ _M2.T


def oklab_to_linear(lab: np.ndarray) -> np.ndarray:
    lms = lab @ _M2_INV.T
    return (lms ** 3) @ _M1_INV.T


def rgb01_to_oklab(rgb: np.ndarray) -> np.ndarray:
    return linear_to_oklab(srgb_to_linear(rgb))


def oklab_to_rgb01(lab: np.ndarray) -> np.ndarray:
    return linear_to_srgb(oklab_to_linear(lab))


def hex_of(rgb) -> str:
    return "#%02X%02X%02X" % tuple(int(round(c)) for c in rgb)


# --------------------------------------------------------------------------- #
# 调色板加载
# --------------------------------------------------------------------------- #


def load_palette(spec: str) -> dict[str, tuple[int, int, int]]:
    """`typical` / `nominal` / 或者一个 json 文件路径。"""
    if spec in PALETTES:
        return dict(PALETTES[spec])

    with open(spec, "r", encoding="utf-8") as f:
        raw = json.load(f)
    # 允许两种写法：{"black":[r,g,b],...} 或 [{"name":..,"rgb":[..]}, ...]
    if isinstance(raw, list):
        raw = {e["name"]: e["rgb"] for e in raw}
    pal = {}
    for name in NAMES:
        if name not in raw:
            raise SystemExit(f"调色板 {spec} 缺少颜色 {name}（需要 6 个键：{NAMES}）")
        pal[name] = tuple(int(c) for c in raw[name][:3])
    return pal


def palette_arrays(pal: dict) -> tuple[np.ndarray, np.ndarray]:
    """返回 (sRGB 0..1 数组 (6,3), OKLab 数组 (6,3))，顺序固定为 NAMES。"""
    rgb = np.array([pal[n] for n in NAMES], dtype=np.float64) / 255.0
    return rgb, rgb01_to_oklab(rgb)


# --------------------------------------------------------------------------- #
# 图像准备：裁切 / 缩放 / 色调映射
# --------------------------------------------------------------------------- #


def fit_image(img: Image.Image, fit: str, focus: tuple[float, float]) -> Image.Image:
    """把任意长宽比塞进 480x720。"""
    if fit == "stretch":
        return img.resize((W, H), Image.LANCZOS)

    sw, sh = img.size
    target = W / H

    if fit == "crop":
        # 以 focus 为中心尽量少裁
        if sw / sh > target:                     # 太宽 -> 裁两边
            nw = int(round(sh * target))
            nh = sh
        else:                                    # 太高 -> 裁上下
            nw = sw
            nh = int(round(sw / target))
        fx, fy = focus
        x0 = int(round((sw - nw) * fx))
        y0 = int(round((sh - nh) * fy))
        box = (x0, y0, x0 + nw, y0 + nh)
        return img.crop(box).resize((W, H), Image.LANCZOS)

    if fit == "contain":
        # 等比缩放后补白（留边），照片不会丢内容
        scale = min(W / sw, H / sh)
        nw, nh = max(1, int(sw * scale)), max(1, int(sh * scale))
        canvas = Image.new("RGB", (W, H), (255, 255, 255))
        canvas.paste(img.resize((nw, nh), Image.LANCZOS), ((W - nw) // 2, (H - nh) // 2))
        return canvas

    raise SystemExit(f"未知的 --fit：{fit}（可选 crop / contain / stretch）")


def tone_map(
    lab: np.ndarray,
    exposure: float,
    contrast: float,
    gamma: float,
    saturation: float,
    shadows: float,
    highlights: float,
) -> np.ndarray:
    """在 OKLab 里做色调映射（曝光已在线性域做掉）。"""
    L = lab[..., 0].copy()
    a = lab[..., 1].copy()
    b = lab[..., 2].copy()

    # 对比：绕中灰拉伸。0=不变，正数增强。
    if contrast:
        L = 0.5 + (L - 0.5) * (1.0 + contrast)

    # 伽马：只压 L，保持色相。gamma<1 提亮。
    if gamma != 1.0:
        L = np.clip(L, 0.0, 1.0) ** gamma

    # 暗部 / 亮部：S 曲线，模拟「把死黑抬起来、把高光压回来」。
    # 电子纸的暗部特别容易糊，抬一点暗部换来细节。
    if shadows:
        L = L + shadows * (1.0 - L) ** 3 * L
    if highlights:
        L = L - highlights * L**3 * (1.0 - L)

    # 饱和：直接缩放 a/b。电子纸色域窄，略加饱和能抵消「灰」的观感。
    if saturation != 1.0:
        a *= saturation
        b *= saturation

    return np.stack([np.clip(L, 0.0, 1.0), a, b], axis=-1)


def compress_lightness(lab: np.ndarray, pal_lab: np.ndarray) -> np.ndarray:
    """
    把图像的 L 范围压到面板白/黑之间。

    这一步经常被忽略，但它决定了「整张图是通透还是糊成一团」：
    面板的白 L≈0.93 而不是 1.0，黑 L≈0.10 而不是 0。
    不压缩的话，图里所有 L>0.93 的像素（天空、白墙）都被判成同一批颜色，
    亮部直接变成一块死白；暗部同理。
    """
    lo = float(pal_lab[:, 0].min())
    hi = float(pal_lab[:, 0].max())
    out = lab.copy()
    out[..., 0] = lo + (hi - lo) * np.clip(lab[..., 0], 0.0, 1.0)
    return out


# --------------------------------------------------------------------------- #
# 抖动算法
# --------------------------------------------------------------------------- #

KERNELS = {
    # Floyd–Steinberg：经典，4 邻域，速度快，暗部易出「虫线」
    "fs": [(0, 1, 7 / 16), (1, -1, 3 / 16), (1, 0, 5 / 16), (1, 1, 1 / 16)],
    # Stucki：12 邻域，误差摊得更匀，照片细节/渐变最好，是本工具的默认
    #   权重 /42：  (0,1)=8 (0,2)=4
    #               (1,-2..2)=2,4,8,4,2
    #               (2,-2..2)=1,2,4,2,1     （合计 42/42，必须摊满，否则整张会发灰）
    "stucki": [
        (0, 1, 8 / 42), (0, 2, 4 / 42),
        (1, -2, 2 / 42), (1, -1, 4 / 42), (1, 0, 8 / 42), (1, 1, 4 / 42), (1, 2, 2 / 42),
        (2, -2, 1 / 42), (2, -1, 2 / 42), (2, 0, 4 / 42), (2, 1, 2 / 42), (2, 2, 1 / 42),
    ],
    # Atkinson：只扩散 6/8 的误差，对比度高、留白多，适合线稿/logo，不适合照片
    "atkinson": [(0, 1, 1 / 8), (0, 2, 1 / 8), (1, -1, 1 / 8),
                 (1, 0, 1 / 8), (1, 1, 1 / 8), (2, 0, 1 / 8)],
}

#: 8x8 Bayer 有序抖动矩阵（值域 0..1）
BAYER8 = np.array(
    [
        [0, 32, 8, 40, 2, 34, 10, 42],
        [48, 16, 56, 24, 50, 18, 58, 26],
        [12, 44, 4, 36, 14, 46, 6, 38],
        [60, 28, 52, 20, 62, 30, 54, 22],
        [3, 35, 11, 43, 1, 33, 9, 41],
        [51, 19, 59, 27, 49, 17, 57, 25],
        [15, 47, 7, 39, 13, 45, 5, 37],
        [63, 31, 55, 23, 61, 29, 53, 21],
    ],
    dtype=np.float64,
) / 64.0


def nearest_codes(lab: np.ndarray, pal_lab: np.ndarray,
                  weights=(1.0, 0.9, 0.9)) -> np.ndarray:
    """纯量化：每个像素取 OKLab 里最近的一色（带轻微通道权重，按人眼色敏）。"""
    w = np.asarray(weights)[None, None, :]
    d = ((lab[None] - pal_lab[:, None, None, :]) * w) ** 2
    return np.argmin(d.sum(-1), axis=0).astype(np.uint8)


def ordered_codes(lab: np.ndarray, pal_lab: np.ndarray, amp: float = 0.55) -> np.ndarray:
    """
    有序抖动（8x8 Bayer）。

    做法：按调色板在 OKLab 里的平均间距生成一个阈值扰动，叠到像素值上再取最近色。
    结果是规则的网点 —— 渐变干净、无方向性噪点，但会看到「布纹」。
    """
    # 调色板两两距离的中位数，作为「一个色阶」的尺度
    d = np.sqrt(((pal_lab[:, None, :] - pal_lab[None, :, :]) ** 2).sum(-1))
    spacing = float(np.median(d[d > 1e-6]))

    th = BAYER8[np.arange(H)[:, None] % 8, np.arange(W)[None, :] % 8] - 0.5
    th = th[..., None] * (amp * spacing)

    # 扰动方向沿着「局部残差」而非各向同性：色相更稳
    codes0 = nearest_codes(lab, pal_lab)
    resid = lab - pal_lab[codes0]
    n = np.linalg.norm(resid, axis=-1, keepdims=True) + 1e-6
    jitter = th * (resid / n)
    # 加上各向同性分量，避免纯色平铺时出现色块边界
    jitter = jitter * 0.8 + th * 0.2
    return nearest_codes(lab + jitter, pal_lab)


def diffusion_codes(lab: np.ndarray, pal_lab: np.ndarray, kernel: str,
                    serpentine: bool = True, err_clamp: float = 0.30,
                    strength: float = 1.0) -> np.ndarray:
    """
    误差扩散（逐像素串行）。

    纯 Python 循环 34.5 万个像素，约几秒 —— 抖动本身就是串行算法，
    想更快可以装 numba，但没必要为了几秒引入依赖。
    """
    k = KERNELS[kernel]
    h, w = lab.shape[:2]
    pl = [float(v) for v in pal_lab[:, 0]]
    pa = [float(v) for v in pal_lab[:, 1]]
    pb = [float(v) for v in pal_lab[:, 2]]

    L = lab[..., 0].ravel().tolist()
    A = lab[..., 1].ravel().tolist()
    B = lab[..., 2].ravel().tolist()
    n = h * w
    eL = [0.0] * n
    eA = [0.0] * n
    eB = [0.0] * n
    out = bytearray(n)

    for y in range(h):
        row = y * w
        left_to_right = (y % 2 == 0) or not serpentine
        # 蛇形扫描：偶数行左→右，奇数行右→左，抵消方向性条纹
        koff = [(dy * w + (dx if left_to_right else -dx), wt) for dy, dx, wt in k]
        xs = range(w) if left_to_right else range(w - 1, -1, -1)

        for x in xs:
            i = row + x
            l = L[i] + eL[i]
            a = A[i] + eA[i]
            b = B[i] + eB[i]

            best = 0
            bd = 1e30
            for c in range(6):
                dl = l - pl[c]
                da = (a - pa[c]) * 0.9
                db = (b - pb[c]) * 0.9
                dd = dl * dl + da * da + db * db
                if dd < bd:
                    bd = dd
                    best = c
            out[i] = best

            dl = (l - pl[best]) * strength
            da = (a - pa[best]) * strength
            db = (b - pb[best]) * strength
            if dl > err_clamp:
                dl = err_clamp
            elif dl < -err_clamp:
                dl = -err_clamp
            if da > err_clamp:
                da = err_clamp
            elif da < -err_clamp:
                da = -err_clamp
            if db > err_clamp:
                db = err_clamp
            elif db < -err_clamp:
                db = -err_clamp

            for off, wt in koff:
                j = i + off
                if 0 <= j < n:
                    eL[j] += dl * wt
                    eA[j] += da * wt
                    eB[j] += db * wt

    return np.frombuffer(bytes(out), dtype=np.uint8).reshape(h, w)


# --------------------------------------------------------------------------- #
# DBS（Direct Binary Search）细化
# --------------------------------------------------------------------------- #
#
# 抖动是「贪心」的：只保证局部误差被摊掉，不保证整片区域最优。
# DBS 换一个判据：把「人眼看到的东西」建模成高斯模糊，
# 然后反复问 —— 把这个像素换个颜色，模糊后的整体误差会不会更小？
#
# 目标函数  E = Σ ‖ Gσ * (T − R) ‖²
# 单个像素由 c0 改成 c 时，误差变化只跟两个量有关：
#   B = Gσ * Gσ * (T − R)     （把误差场再模糊一次）
#   κ = Σ Gσ²                  （常数）
#   ΔE(c) = 2·(c0−c)·B + κ·‖c0−c‖²
# 于是每轮只要 1 次 FFT 模糊 + 6 个候选色的一次比较，非常快。
#
# ⚠️ 一个实做上的坑：这个 ΔE 只对一个像素成立。如果每轮把所有像素一起改，
# 邻域影响会互相叠加（1+1 > 2），整幅图就在两个状态之间来回振荡，
# 最后收敛到一个噪声化的解 —— 实测反而比 Stucki 更差。
# 所以每轮只随机挑一小部分「有正收益」的像素落地（≈6%），跑 30 轮：
# 误差场单调下降，ΔE_OK 从 7.5 降到 5.2 左右。


def _gauss_blur(arr: np.ndarray, sigma: float) -> np.ndarray:
    h, w = arr.shape[:2]
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    g = np.exp(-2.0 * (math.pi**2) * sigma**2 * (fx**2 + fy**2))
    if arr.ndim == 3:
        g = g[..., None]
    spec = np.fft.fft2(arr, axes=(0, 1))
    return np.real(np.fft.ifft2(spec * g, axes=(0, 1)))


def _kappa(sigma: float, shape=(H, W)) -> float:
    imp = np.zeros(shape)
    imp[0, 0] = 1.0
    k = _gauss_blur(imp, sigma)
    return float((k * k).sum())


def dbs_refine(codes: np.ndarray, target_lab: np.ndarray, pal_lab: np.ndarray,
               passes: int = 30, sigma: float = 1.4, keep: float = 0.06,
               seed: int = 1, verbose: bool = True) -> np.ndarray:
    """
    DBS 细化。每轮只落地 keep 比例的改进（约 6%），避免并行更新振荡。
    """
    codes = codes.copy()
    kappa = _kappa(sigma)
    rng = np.random.default_rng(seed)
    R = pal_lab[codes]                      # 当前渲染结果（OKLab）
    t0 = time.time()

    for p in range(passes):
        D = target_lab - R
        Bf = _gauss_blur(_gauss_blur(D, sigma), sigma)

        cost = np.zeros(codes.shape)          # 不动 = 0
        best = codes.copy()
        for c in range(6):
            delta = R - pal_lab[c]            # (h,w,3)
            cs = 2.0 * (delta * Bf).sum(-1) + kappa * (delta * delta).sum(-1)
            better = cs < cost - 1e-12
            cost = np.where(better, cs, cost)
            best = np.where(better, np.uint8(c), best)

        sel = (rng.random(codes.shape) < keep) & (cost < 0.0)
        codes = np.where(sel, best, codes)
        R = pal_lab[codes]

        if verbose and (p + 1) % 10 == 0:
            err = float(((_gauss_blur(target_lab - R, sigma)) ** 2).sum())
            print(f"    DBS {p + 1:2d}/{passes} 轮  感知误差 {err:8.1f}  "
                  f"{time.time() - t0:4.1f}s", flush=True)

    return codes


# --------------------------------------------------------------------------- #
# 打包 / 预览 / 评价
# --------------------------------------------------------------------------- #


#: 调色板序号 -> 硬件色值。序号是 0..5 连续的（算法内部方便），
#: 但面板的 4bit 编码不是连续的：0x4 是非法值，蓝绿被挪到了 5、6。
#: 【必须映射】直接把序号写进帧里，蓝会变成非法值 0x4、绿会画成蓝。
IDX_TO_HW = np.array([CODE[n] for n in NAMES], dtype=np.uint8)   # [0,1,2,3,5,6]
HW_TO_IDX = np.zeros(16, dtype=np.uint8)                          # 反向表
for _i, _hw in enumerate(IDX_TO_HW):
    HW_TO_IDX[_hw] = _i
HW_TO_IDX[0x4] = 0                                                 # 非法值按黑处理


def pack_frame(codes: np.ndarray, swap_nibble: bool = False) -> bytes:
    """
    codes[h][w]（调色板序号 0..5，y=0 是屏幕顶）-> 172800 字节。

    硬件事实（见 docs/03 第 4 节）：
      * 列优先：480 条竖列 × 360 字节，一条列连续存放；
      * 列内自下往上：第 0 个字节是玻璃最下面那一对像素；
      * 一字节 = 同列相邻的两个像素，**高半字节在玻璃上偏下**；
      * 每个半字节必须是**硬件色值**（0,1,2,3,5,6），见 IDX_TO_HW。
    """
    hw = IDX_TO_HW[codes]
    fy = hw[::-1]                             # fy=0 对应最底部
    hi = fy[0::2]                             # fy 偶数 -> 高半字节
    lo = fy[1::2]                             # fy 奇数 -> 低半字节
    if swap_nibble:
        hi, lo = lo, hi
    buf = ((hi.astype(np.uint8) << 4) | lo.astype(np.uint8)).T  # (480,360)
    data = np.ascontiguousarray(buf).tobytes()
    assert len(data) == FRAME_BYTES, len(data)
    return data


def unpack_frame(data: bytes, swap_nibble: bool = False) -> np.ndarray:
    """反向解析（还原成调色板序号），用于自检 / 把 .bin 转回 PNG。"""
    if len(data) != FRAME_BYTES:
        raise ValueError(f"不是本屏的帧数据：{len(data)} B，应为 {FRAME_BYTES} B"
                         f"（本工具生成的头文件请先按 0x.. 解析成字节再传进来）")
    buf = np.frombuffer(data, dtype=np.uint8).reshape(W, COL_BYTES).T  # (360,480)
    hi = (buf >> 4) & 0x0F
    lo = buf & 0x0F
    if swap_nibble:
        hi, lo = lo, hi
    fy = np.empty((H, W), dtype=np.uint8)
    fy[0::2] = HW_TO_IDX[hi]
    fy[1::2] = HW_TO_IDX[lo]
    return fy[::-1]


def render(codes: np.ndarray, pal_rgb: np.ndarray) -> np.ndarray:
    """把色号渲染成观感 RGB（0..1）。"""
    return pal_rgb[codes]


def perceived(lab: np.ndarray, sigma: float) -> np.ndarray:
    """模拟人眼在正常观看距离上的低通。"""
    return _gauss_blur(lab, sigma)


def delta_e_ok(codes: np.ndarray, target_lab: np.ndarray, pal_lab: np.ndarray,
               sigma: float = 1.6) -> float:
    """
    感知色差：两边都模糊后再比 OKLab 距离，×100 当作「ΔE」。

    注意：**不要**对抖动图算逐像素 PSNR —— 抖动图逐像素误差天然很大，
    但看起来完全一样。必须先过一遍眼模型，比较才有意义。
    """
    r = perceived(pal_lab[codes], sigma)
    t = perceived(target_lab, sigma)
    d = np.sqrt(((r - t) ** 2).sum(-1))
    return float(np.mean(d) * 100.0)


# --------------------------------------------------------------------------- #
# 字体 / 对比图
# --------------------------------------------------------------------------- #


def _font(size: int):
    for path in (
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
    ):
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            continue
    return ImageFont.load_default()


def to_pil(rgb01: np.ndarray) -> Image.Image:
    return Image.fromarray((np.clip(rgb01, 0, 1) * 255 + 0.5).astype(np.uint8))


def make_compare(items, path, cols=None):
    """
    items: [(标题, PIL.Image, 备注), ...] —— 排成对比图。
    """
    cols = cols or len(items)
    cw, ch = 360, 540
    pad, top = 14, 74
    rows = int(math.ceil(len(items) / cols))
    sheet = Image.new("RGB", (cols * (cw + pad) + pad,
                              rows * (ch + top + pad) + pad), (26, 26, 30))
    d = ImageDraw.Draw(sheet)
    f1, f2 = _font(22), _font(17)
    for i, (title, im, sub) in enumerate(items):
        r, c = divmod(i, cols)
        x = pad + c * (cw + pad)
        y = pad + r * (ch + top + pad)
        sheet.paste(im.resize((cw, ch), Image.LANCZOS), (x, y + top))
        d.text((x, y + 8), title, fill=(240, 240, 240), font=f1)
        d.text((x, y + 38), sub, fill=(150, 190, 150), font=f2)
    sheet.save(path)


# --------------------------------------------------------------------------- #
# C 头文件（把帧数据编进固件，不用传文件）
# --------------------------------------------------------------------------- #


def write_header(path: str, data: bytes, name: str = "photo_frame",
                 guarded: bool = True, per_line: int = 16,
                 comment: str = "") -> None:
    """写成 Arduino/ESP32 能直接 #include 的 .h。

    尽量省 Flash 与编译时间：
      * const + PROGMEM，落在 flash 的 .rodata，不占 RAM
      * 每行 per_line 个字节，10800 行、约 1 MB 源码；用 16 而不是 24
        是为了任何编辑器都能秒开（24 也完全可以）
      * guarded=True 时加 #pragma once，作为独立 .h 被多次包含也安全
    """
    lines = []
    if comment:
        for c in comment.splitlines():
            lines.append("// " + c if c else "//")
    else:
        lines.append("// generated by tools/epd_photo.py")
    lines.append(f"// {FRAME_BYTES} B, 480x720, 4bit/px, column-major + Y-mirrored")
    lines.append("// 编进 flash 的 .rodata，不占 RAM；用 memcpy_P / 直接读都可以")
    if guarded:
        lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#ifndef PROGMEM")
    lines.append("#define PROGMEM")
    lines.append("#endif")
    lines.append("")
    lines.append(f"#define {name.upper()}_W      {W}")
    lines.append(f"#define {name.upper()}_H      {H}")
    lines.append(f"#define {name.upper()}_BYTES  {FRAME_BYTES}")
    lines.append("")
    # 一维 16 字节一行：视觉上仍是「320 组 × 540 行」的方格阵列
    lines.append(f"const uint8_t {name}[{FRAME_BYTES}] PROGMEM = {{")
    for i in range(0, FRAME_BYTES, per_line):
        chunk = data[i:i + per_line]
        lines.append("  " + ",".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


# --------------------------------------------------------------------------- #
# 主流程
# --------------------------------------------------------------------------- #


def process(img: Image.Image, args, pal_rgb, pal_lab) -> tuple[np.ndarray, np.ndarray]:
    """返回 (codes, target_lab)。target_lab 是色调映射后、量化前的理想画面。"""
    img = fit_image(img, args.fit, tuple(args.focus))

    arr = np.asarray(img.convert("RGB"), dtype=np.float64) / 255.0

    # 曝光在线性域做，否则暗部会被压死
    if args.exposure:
        arr = np.clip(srgb_to_linear(arr) * (2.0 ** args.exposure), 0.0, 1.0)
        arr = linear_to_srgb(arr)

    lab = tone_map(
        rgb01_to_oklab(arr),
        exposure=0.0,
        contrast=args.contrast,
        gamma=args.gamma,
        saturation=args.saturation,
        shadows=args.shadows,
        highlights=args.highlights,
    )
    lab = compress_lightness(lab, pal_lab)

    m = args.method
    if m == "nearest":
        codes = nearest_codes(lab, pal_lab)
    elif m == "bayer":
        codes = ordered_codes(lab, pal_lab, amp=args.ordered_amp)
    elif m in KERNELS:
        codes = diffusion_codes(lab, pal_lab, m, serpentine=not args.no_serpentine,
                                err_clamp=args.err_clamp, strength=args.diffusion_strength)
    elif m == "dbs":
        base = args.dbs_seed
        if base in KERNELS:
            codes = diffusion_codes(lab, pal_lab, base,
                                    serpentine=not args.no_serpentine,
                                    err_clamp=args.err_clamp,
                                    strength=args.diffusion_strength)
        else:
            codes = nearest_codes(lab, pal_lab)
        codes = dbs_refine(codes, lab, pal_lab, passes=args.dbs_passes,
                           sigma=args.dbs_sigma)
    else:
        raise SystemExit(f"未知的 --method：{m}")

    return codes, lab


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="把彩色照片转成 OSPTEK 3.7\" E6 480x720 六色墨水屏的帧数据",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("input", help="输入图片（jpg / png / heic...由 Pillow 决定）")
    ap.add_argument("-o", "--output", help="输出 .bin（172800 字节原始帧）")
    ap.add_argument("--preview", help="输出「面板上看到的样子」PNG")
    ap.add_argument("--sim", help="输出「正常距离观看」的模拟图 PNG（模糊后放大）")
    ap.add_argument("--compare", help="输出多算法对比图 PNG")
    ap.add_argument("--compare-from", default="",
                    help="对比图里的已有 .bin 路径（逗号分隔），用于回看实拍结果")

    # 版面
    ap.add_argument("--fit", default="crop", choices=["crop", "contain", "stretch"])
    ap.add_argument("--focus", nargs=2, type=float, default=[0.5, 0.5],
                    metavar=("FX", "FY"), help="裁切锚点，0=左/上，1=右/下")
    ap.add_argument("--mount", type=int, default=0,
                    help="四周留白像素（照片装裱感），会先留白再缩放")

    # 色调
    ap.add_argument("--exposure", type=float, default=0.0, help="曝光，单位 EV，如 0.4")
    ap.add_argument("--contrast", type=float, default=0.10, help="对比，0=不变，0.1≈+10%%")
    ap.add_argument("--gamma", type=float, default=1.0, help="<1 提亮")
    ap.add_argument("--saturation", type=float, default=1.12, help="饱和倍数，电子纸建议 1.1~1.25")
    ap.add_argument("--shadows", type=float, default=0.0, help="抬暗部，0~0.5")
    ap.add_argument("--highlights", type=float, default=0.0, help="压高光，0~0.5")
    ap.add_argument("--palette", default="typical",
                    help="typical（默认，按观感标定）/ nominal（原始 0-255）/ xxx.json")

    # 抖动
    ap.add_argument("--method", default="dbs",
                    choices=["nearest", "bayer", "fs", "stucki", "atkinson", "dbs"],
                    help="dbs = 误差扩散起步 + DBS 细化（默认，效果最好也最慢）")
    ap.add_argument("--no-serpentine", action="store_true", help="关闭蛇形扫描")
    ap.add_argument("--err-clamp", type=float, default=0.30, help="误差限幅，越小越干净")
    ap.add_argument("--diffusion-strength", type=float, default=1.0, help="误差扩散强度")
    ap.add_argument("--ordered-amp", type=float, default=0.55, help="有序抖动幅度")
    ap.add_argument("--dbs-seed", default="fs",
                    help="DBS 的起点算法（fs 最好，stucki 次之）")
    ap.add_argument("--dbs-passes", type=int, default=20,
                    help="DBS 轮数。20 轮已吃掉大部分收益，加轮数只多 0.01~0.02")
    ap.add_argument("--dbs-sigma", type=float, default=1.4,
                    help="眼模型模糊半径（像素）。1.2~1.8 比较甜，越大越偏整体色调、越小越保细节")

    # 输出 / 集成
    ap.add_argument("--header",
                    help="生成可直接 #include 的 C 头文件（PROGMEM 数组，编进固件，不用传文件）")
    ap.add_argument("--header-name", default="photo_frame",
                    help="数组名，默认 photo_frame")
    ap.add_argument("--swap-nibble", action="store_true",
                    help="半字节顺序与固件 EPD_SWAP_NIBBLE=1 对应")
    ap.add_argument("--variants", help="输出目录：一次生成 4 种算法 + 对比图 + bin")
    ap.add_argument("--quiet", action="store_true")

    args = ap.parse_args(argv)

    pal = load_palette(args.palette)
    pal_rgb, pal_lab = palette_arrays(pal)
    # 预览永远按观感色渲染（否则 nominal 的预览会骗人）
    view_rgb, _ = palette_arrays(PALETTES["typical"])

    img = Image.open(args.input)
    img = ImageOps.exif_transpose(img).convert("RGB")

    if args.mount > 0:
        canvas = Image.new("RGB", (img.width + 2 * args.mount, img.height + 2 * args.mount),
                           (250, 250, 246))
        canvas.paste(img, (args.mount, args.mount))
        img = canvas

    log = (lambda *a, **k: None) if args.quiet else print

    log(f"输入 {args.input}  {img.width}x{img.height}")
    log(f"调色板 {args.palette}: " + "  ".join(
        f"{n}={hex_of(pal[n])}" for n in NAMES))
    log(f"算法 {args.method}")

    if args.variants:
        os.makedirs(args.variants, exist_ok=True)
        methods = [("nearest", "NEAREST", "no dithering - baseline"),
                   ("bayer", "ORDERED 8x8", "regular screen pattern"),
                   ("fs", "FLOYD-STEINBERG", "error diffusion, serpentine"),
                   ("dbs", "FS + DBS", "perceptual refinement (best)")]
        items, best = [], None
        for m, title, sub in methods:
            args.method = m
            t0 = time.time()
            codes, lab = process(img, args, pal_rgb, pal_lab)
            de = delta_e_ok(codes, lab, pal_lab, args.dbs_sigma)
            ms = int((time.time() - t0) * 1000)
            fn = os.path.join(args.variants, f"{m}.bin")
            with open(fn, "wb") as f:
                f.write(pack_frame(codes, args.swap_nibble))
            pv = to_pil(render(codes, view_rgb))
            pv.save(os.path.join(args.variants, f"{m}.png"))
            log(f"  {m:<9} ΔE_OK={de:5.2f}  {ms:5d} ms  -> {fn}")
            items.append((title, pv, f"{sub}   dE={de:.2f}"))
            if best is None:
                best = (m, de)
            elif de < best[1]:
                best = (m, de)

        # 对比图：左原图 + 4 种算法
        orig = to_pil(np.asarray(fit_image(img, args.fit, tuple(args.focus)),
                                 dtype=np.float64) / 255.0)
        make_compare([("ORIGINAL", orig, "source photo")] + items,
                     os.path.join(args.variants, "compare.png"))
        log(f"最好：{best[0]}（ΔE_OK={best[1]:.2f}）；对比图 -> compare.png")
        return 0

    t0 = time.time()
    codes, lab = process(img, args, pal_rgb, pal_lab)
    de = delta_e_ok(codes, lab, pal_lab, args.dbs_sigma)
    log(f"完成 {int((time.time() - t0) * 1000)} ms，"
        f"感知色差 ΔE_OK={de:.2f}")

    data = pack_frame(codes, args.swap_nibble)

    if args.output:
        with open(args.output, "wb") as f:
            f.write(data)
        log(f"帧数据 -> {args.output}（{len(data)} B）")

    if args.preview:
        to_pil(render(codes, view_rgb)).save(args.preview)
        log(f"面板观感预览 -> {args.preview}")

    if args.sim:
        small = perceived(pal_lab[codes], args.dbs_sigma)
        rgb = oklab_to_rgb01(small)
        to_pil(rgb).resize((W * 2, H * 2), Image.LANCZOS).save(args.sim)
        log(f"观看距离模拟 -> {args.sim}")

    if args.compare:
        items = [("ORIGINAL", to_pil(np.asarray(fit_image(img, args.fit, tuple(args.focus)),
                                               dtype=np.float64) / 255.0), "source photo"),
                 (f"{args.method.upper()}", to_pil(render(codes, view_rgb)),
                  f"selected   dE={de:.2f}")]
        for path in [p for p in args.compare_from.split(",") if p]:
            with open(path, "rb") as f:
                raw = f.read()
            if len(raw) != FRAME_BYTES:
                raise SystemExit(f"--compare-from: {path} 有 {len(raw)} B，"
                                 f"不是本屏的帧数据（应为 {FRAME_BYTES} B）")
            c = unpack_frame(raw, args.swap_nibble)
            items.append((os.path.basename(path), to_pil(render(c, view_rgb)),
                          f"dE={delta_e_ok(c, lab, pal_lab, args.dbs_sigma):.2f}"))
        make_compare(items, args.compare)

    if args.header:
        write_header(args.header, data, args.header_name,
                     comment=f"{os.path.basename(args.input)}  {W}x{H}  "
                             f"method={args.method}  palette={args.palette}")
        log(f"C 数组 -> {args.header}")
        log(f"  数组名 {args.header_name}[{FRAME_BYTES}]，占 "
            f"{FRAME_BYTES / 1024:.0f} KB Flash（不占 RAM）")

    if not (args.output or args.preview or args.header or args.variants or args.compare):
        log("（没有指定输出，用 --header / -o / --preview）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
