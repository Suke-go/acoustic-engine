# Perceptual Parameter Editor

**知覚心理学に基づくオーディオパラメータをリアルタイム調整するGUIツール**

## Overview

Perceptual Parameter Editor は、Acoustic Engine の知覚制御API (`ae_set_perceived_distance()`, `ae_set_perceived_spaciousness()`, `ae_set_perceived_clarity()`) をGUIで操作できるインタラクティブツールです。

```
スライダー操作 → 知覚パラメータ → DLL内部変換 → 物理パラメータ → 音声加工
```

## Requirements

- **Python** 3.8+
- **依存パッケージ**: `sounddevice`, `numpy`, `pydub`, `matplotlib`
- **FFmpeg** (MP3読み込みに必要)

### インストール

```bash
pip install sounddevice numpy pydub matplotlib
```

FFmpegのインストール (Windows):
```powershell
winget install ffmpeg
```

## Usage

```bash
cd acoustic_engine
python perceptual_editor_gui.py
```

---

## Parameter Computation (パラメータ計算)

本エディタでは、3つの知覚パラメータを物理パラメータに変換し、それを用いて音声信号を加工します。

### 知覚 → 物理パラメータ変換

#### Distance (距離感)

```
入力: d ∈ [0.0, 1.0]  (0=近い, 1=遠い)

物理パラメータ:
  physical_distance = 1.0 × 100^d  [m]
  dry_wet = 0.2 + 0.6 × d
  brightness = -0.5 × d
```

| d | physical_distance | dry_wet | brightness |
|---|-------------------|---------|------------|
| 0.0 | 1 m | 0.2 | 0.0 |
| 0.5 | 10 m | 0.5 | -0.25 |
| 1.0 | 100 m | 0.8 | -0.5 |

**根拠**: Zahorik (2002) の研究により、知覚距離とDRR（直接音/残響音比）の対数関係が確認されている。

#### Spaciousness (空間感)

```
入力: s ∈ [0.0, 1.0]  (0=狭い, 1=広い)

物理パラメータ:
  width = 2.0 × s
  room_size = 0.2 + 0.6 × s
```

| s | width | room_size |
|---|-------|-----------|
| 0.0 | 0.0 (mono) | 0.2 |
| 0.5 | 1.0 (stereo) | 0.5 |
| 1.0 | 2.0 (ultra-wide) | 0.8 |

**根拠**: Bradley & Soulodre (1995) によるASW（見かけの音源幅）とIACC（両耳間相関）の研究。

#### Clarity (明瞭度)

```
入力: c ∈ [0.0, 1.0]  (0=こもる, 1=クリア)

物理パラメータ:
  dry_wet = 0.7 - 0.5 × c
  room_size = 0.6 - 0.4 × c
  brightness = 0.3 × c - 0.15
```

| c | dry_wet | room_size | brightness |
|---|---------|-----------|------------|
| 0.0 | 0.7 | 0.6 | -0.15 |
| 0.5 | 0.45 | 0.4 | 0.0 |
| 1.0 | 0.2 | 0.2 | +0.15 |

**根拠**: ISO 3382 の C50/C80（クラリティ）指標。C50 = 10 log₁₀(E₀₋₅₀ₘₛ / E₅₀ₘₛ₊)

---

### 音声信号処理

物理パラメータから実際の音声処理への変換:

#### 1. 距離減衰 (Distance Attenuation)

```
y[n] = x[n] × distance_factor

distance_factor = 1 / (1 + physical_distance × 0.1)
```

逆2乗則の簡略化実装。遠いほど音量が下がる。

#### 2. 高域減衰フィルタ (Brightness)

brightness < 0 の場合、1次IIRローパスフィルタを適用:

```
α = 0.3 + 0.7 × (1 + brightness)
y[n] = α × x[n] + (1 - α) × y[n-1]
```

| brightness | α | カットオフ周波数の傾向 |
|------------|---|---------------------|
| -0.5 | 0.65 | 高域減衰 (暗い音) |
| 0.0 | 1.0 | フィルタなし |

**物理的意味**: 大気/水中の高域吸収をシミュレート

#### 3. ステレオ幅制御 (Width / M-S Processing)

M/S変換によるステレオ幅調整:

```
Mid = (L + R) / 2
Side = (L - R) / 2

Side' = Side × width

L_out = Mid + Side'
R_out = Mid - Side'
```

| width | 効果 |
|-------|-----|
| 0.0 | 完全モノラル (Side = 0) |
| 1.0 | 元のステレオ |
| 2.0 | ステレオ幅2倍 |

#### 4. 残響シミュレーション (Dry/Wet)

シンプルなディレイ+減衰による残響近似:

```
delay_samples = 0.05 × sample_rate  (50ms)
reverb[n] = y[n - delay_samples] × 0.3 × dry_wet

output[n] = y[n] × (1 - dry_wet × 0.5) + reverb[n]
```

| dry_wet | 効果 |
|---------|-----|
| 0.0 | ドライのみ (残響なし) |
| 0.5 | 半々ミックス |
| 1.0 | ウェット優位 (残響多め) |

#### 5. 正規化 (Normalization)

クリッピング防止:

```
max_val = max(|output|)
if max_val > 0.95:
    output = output × (0.95 / max_val)
```

---

### 処理フロー図

```
┌─────────────────────────────────────────────────────────────┐
│                    Audio Processing Chain                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [Input Audio]                                              │
│       │                                                     │
│       ▼                                                     │
│  ┌─────────────────┐                                        │
│  │ Distance Atten. │ ← physical_distance                   │
│  │  y = x × factor │                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │ Brightness LPF  │ ← brightness < 0 の場合               │
│  │  y = αx + (1-α)y│                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │ M/S Width Ctrl  │ ← width                               │
│  │  Side' = S × w  │                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │ Reverb Sim.     │ ← dry_wet                             │
│  │  delay + decay  │                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │ Normalize       │                                        │
│  │  peak → 0.95    │                                        │
│  └────────┬────────┘                                        │
│           ▼                                                 │
│  [Output Audio]                                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Controls

| ボタン | 機能 |
|-------|------|
| **Load** | 音声ファイル読み込み (WAV, MP3) |
| **Play** | 加工後の音声を再生 |
| **Stop** | 再生停止 |
| **Export** | 加工済み音声をWAV形式で保存 |
| **Reset Parameters** | パラメータを初期値 (0.5) にリセット |

> **Note**: スライダーを動かすと自動的に音声が再処理され、波形が更新されます。

## Waveform Display

- **青線 (Original)**: 加工前の波形
- **オレンジ線 (Processed)**: 加工後の波形

パラメータを変更すると、オレンジの波形が変化します。振幅の違い、波形のなまり具合などで効果を視覚的に確認できます。

---

## Presets (参考設定)

### 遠くの広い空間
```
Distance: 0.8, Spaciousness: 0.7, Clarity: 0.3
→ 振幅減少、高域減衰、広いステレオ、残響多め
```

### すぐ近くでダイレクト
```
Distance: 0.1, Spaciousness: 0.3, Clarity: 0.8
→ 振幅維持、高域維持、狭いステレオ、ドライ
```

### 没入感のある包囲
```
Distance: 0.5, Spaciousness: 0.9, Clarity: 0.4
→ 中程度の距離、超ワイドステレオ、やや残響
```

---

## Troubleshooting

| エラー | 解決策 |
|-------|-------|
| `Audio libraries not available` | `pip install sounddevice numpy pydub matplotlib` |
| MP3読み込み失敗 | FFmpegをインストール |
| DLL not found | `cmake --build build --config Release` でビルド |
| 音が出ない | PCの音量設定とオーディオデバイスを確認 |

## Files

| ファイル | 説明 |
|---------|------|
| `perceptual_editor_gui.py` | GUIエディタ本体 |
| `perceptual_editor.py` | コンソール版 (キーボード操作) |
| `sample_sounds/test_heartbeat.mp3` | サンプル音声 |

## References

- Zahorik, P. (2002). Assessing auditory distance perception using virtual acoustics. *JASA*, 111(4), 1832-1846.
- Bradley, J.S. & Soulodre, G.A. (1995). The influence of late arriving energy on spatial impression. *JASA*, 97(4), 2263-2271.
- ISO 3382-1:2009. Acoustics — Measurement of room acoustic parameters.
