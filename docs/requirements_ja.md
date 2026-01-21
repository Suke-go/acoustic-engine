# Acoustic Engine 要件定義書

> **バージョン**: 0.2.0-draft  
> **更新日**: 2026-01-21  
> **ステータス**: AMT互換機能追加

---

## 1. プロジェクト概要

### 1.1 ビジョン

**Acoustic Engine**は、知覚心理学に基づいた音響処理ライブラリです。従来のDAW（Audition, Pro Tools等）が持つ「パラメータ過多」の問題を解決し、**意図ベース（Intent-based）**のシンプルなAPIで高品質な音響処理を実現します。

```
従来のアプローチ:
  「リバーブのRT60を2.3秒、プリディレイを45ms、ダンピングを...」

Acoustic Engineのアプローチ:
  「深海のような音にして」「心拍に同期してリバーブを変調して」
```

### 1.2 ターゲットユーザー

| ユーザー層 | ユースケース |
|-----------|-------------|
| メディアアーティスト | インタラクティブインスタレーション、生体信号連動作品 |
| VR/ARデザイナー | 空間音響素材制作、リアルタイム3Dオーディオ |
| サウンドデザイナー | 映像・ゲーム用音響素材の高速バッチ処理 |
| 研究者 | 聴覚心理学実験、音響シミュレーション |

### 1.3 プロジェクトの特徴

1. **知覚心理学ベース**: IACC、ラウドネス知覚、マスキング効果などの科学的指標に基づく処理
2. **物理モデリング**: Francois-Garrison式など、物理的に正確な音響伝播シミュレーション
3. **意図ベースAPI**: 「distance: 0.7」のようなセマンティック表現で複雑な処理を抽象化
4. **生体信号連携**: 心拍(HR/HRV)を音響パラメータにリアルタイムマッピング
5. **高速処理**: SIMD最適化(SSE/AVX)による低レイテンシ処理

---

## 2. リファレンスシナリオ

Acoustic Engineは、複数の音響シナリオをサポートします。各シナリオは物理的・知覚的に正確なパラメータセットとして定義され、ブレンド（混合）も可能です。

### 2.1 シナリオ一覧

#### 2.1.1 自然環境系シナリオ

| シナリオ | 物理的特徴 | 主な処理 | 用途例 |
|---------|-----------|---------|--------|
| 🌊 **deep_sea** | 高域吸収、長い残響、圧力 | Francois-Garrison, FDN (RT60: 5-10秒) | 水中シーン、瞑想 |
| 🌲 **forest** | 散乱・吸収、短い減衰 | Early Reflections重視, 高域軽減衰 | 自然環境、ASMR |
| 🏔️ **cave** | 非常に長い残響、フラッターエコー | FDN (RT60: 3-8秒), 金属的ER | ホラー、探検 |
| 🏜️ **open_field** | 反射なし、距離減衰のみ | 大気吸収 (ISO 9613), 無残響 | 屋外、孤独感 |
| 🌌 **space** | 無音/極低周波のみ | 極端な高域減衰, ドローン追加 | SF、浮遊感 |

#### 2.1.2 人工環境系シナリオ

| シナリオ | 物理的特徴 | 主な処理 | 用途例 |
|---------|-----------|---------|--------|
| 🏛️ **cathedral** | 超長残響 (5-12秒) | FDN高拡散, 低ダンピング | 荘厳、宗教的 |
| 🏢 **office** | 短い残響、吸音多 | 短いER, 高ダンピング | 現代的、日常 |
| 🚇 **tunnel** | 金属的響き、フラッター | 短いディレイ反復, 共鳴 | 工業的、不穏 |
| 🏠 **small_room** | 親密な残響 (0.3-0.8秒) | 近接ER, 適度な拡散 | 対話、インタビュー |
| 📻 **radio** | 帯域制限、ノイズ | 300-5000Hz BP, ビット圧縮 | レトロ、通信 |
| 📞 **telephone** | 狭帯域、歪み | 300-3400Hz BP, 軽い歪み | 会話、距離感 |

#### 2.1.3 感情・芸術表現系シナリオ

| シナリオ | 知覚的効果 | 主な処理 | 用途例 |
|---------|-----------|---------|--------|
| 😰 **tension** | 不安、圧迫感 | 高域強調, 不協和音レゾナンス, コンプ強 | ホラー、サスペンス |
| 🌅 **nostalgia** | 懐かしさ、温かみ | ローファイ, ワウフラッター, 高域ロールオフ | 回想、ノスタルジック |
| 💭 **dream** | 非現実感、浮遊感 | スローピッチ変調, ロングリバーブ, デコリレーション | 夢、記憶 |
| 💓 **intimate** | 近さ、ASMR的 | 近接効果（低域増強）, ステレオ狭め, Dry優位 | 囁き、親密 |
| ⚡ **chaos** | 混乱、破壊 | バッファグリッチ, ビットクラッシュ, 非同期変調 | グリッチアート |
| ✨ **ethereal** | 神秘的、超越 | シマーリバーブ, ピッチシフト層, 高い拡散 | スピリチュアル |

---

### 2.2 シナリオComposerアーキテクチャ

複数のシナリオをリアルタイムでブレンドし、動的な音響空間を構築します。

```
┌─────────────────────────────────────────────────────────────────┐
│                    Scenario Composer Architecture               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Scenario Presets                       │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────┐   │   │
│  │  │ deep_sea│ │  cave   │ │ forest  │ │ cathedral   │   │   │
│  │  └────┬────┘ └────┬────┘ └────┬────┘ └──────┬──────┘   │   │
│  │       └───────────┴───────────┴─────────────┘           │   │
│  │                         │                                │   │
│  │                         ▼                                │   │
│  │              ┌───────────────────┐                       │   │
│  │              │ Scenario Blender  │ ◀── ブレンド比率      │   │
│  │              │ deep_sea: 0.6     │                       │   │
│  │              │ cave: 0.3         │                       │   │
│  │              │ tension: 0.4      │ ◀── 感情レイヤー      │   │
│  │              └─────────┬─────────┘                       │   │
│  │                        │                                 │   │
│  │                        ▼                                 │   │
│  │              ┌───────────────────┐                       │   │
│  │              │ Parameter Resolver│ ◀── パラメータ合成    │   │
│  │              │ RT60, absorption, │                       │   │
│  │              │ EQ, width, etc.   │                       │   │
│  │              └─────────┬─────────┘                       │   │
│  └────────────────────────┼─────────────────────────────────┘   │
│                           ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Processing Chain                       │   │
│  │  Propagation → Spatial → Reverb → Dynamics → EQ → Out   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Biosignal Input                        │   │
│  │  Heart Rate ──▶ Blender比率を動的変調                    │   │
│  │  HRV ──────────▶ chaos/tension レイヤー強度              │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 シナリオAPI

```c
// シナリオの適用（単一）
ae_result_t ae_apply_scenario(ae_engine_t* engine,
                              const char* scenario_name,
                              float intensity);  // 0.0 - 1.0

// シナリオのブレンド（複数）
ae_result_t ae_blend_scenarios(ae_engine_t* engine,
                               const ae_scenario_blend_t* blends,
                               size_t count);

// ブレンド定義
typedef struct {
    const char* name;       // シナリオ名
    float       weight;     // ブレンド重み (0.0 - 1.0)
} ae_scenario_blend_t;

// 使用例
ae_scenario_blend_t blends[] = {
    { "deep_sea", 0.6f },
    { "cave", 0.3f },
    { "tension", 0.4f }
};
ae_blend_scenarios(engine, blends, 3);
```

---

### 2.4 リファレンス実装: 深海シナリオ (deep_sea)

設計の具体的な指針として、「深海のような音」を作成する処理チェーンを詳細に定義します。

#### 2.4.1 処理フロー

```
┌─────────────────────────────────────────────────────────────────┐
│                    Deep Sea Processing Chain                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [Input: Mono] ──▶ ┌─────────────────────────────────────────┐  │
│                    │ 1. HRTF Spatializer                     │  │
│                    │    - 方向補間 + クロスフェード           │  │
│                    │    - 水平スイープモーション             │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 2. Physical Absorption                  │  │
│                    │    - Francois-Garrison式                │  │
│                    │    - 距離依存の高域減衰                  │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 3. Reverb (FDN)                         │  │
│                    │    - Early Reflections (数十ms)         │  │
│                    │    - Late Reverb (RT60: ~8秒)           │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 4. M/S Width Control                    │  │
│                    │    - 原音: Mid寄り (狭く)               │  │
│                    │    - 残響: Side強調 (広く)              │  │
│                    │    - IACC低減による包囲感               │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 5. Sidechain Ducking                    │  │
│                    │    - 原音が鳴っている時に残響を抑制     │  │
│                    │    - マルチバンド対応                   │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 6. Mastering Chain                      │  │
│                    │    - De-esser (シビランス抑制)          │  │
│                    │    - Tilt EQ (高域勾配)                 │  │
│                    │    - Lookahead Limiter                  │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                             [Output: Stereo]                    │
└─────────────────────────────────────────────────────────────────┘
```

#### 2.4.2 deep_sea シナリオのパラメータ

| モジュール | パラメータ | 値 | 根拠 |
|-----------|-----------|-----|------|
| Propagation | absorption_model | francois_garrison | 海水の物理モデル |
| | distance | 50m | 中程度の距離感 |
| | temperature | 5°C | 深海の低温 |
| | depth | 200m | 深海想定 |
| Reverb | rt60 | 8.0s | 巨大な水中空間 |
| | pre_delay | 80ms | 反射までの距離 |
| | damping | 0.7 | 高域の急速減衰 |
| | diffusion | 0.85 | 高い拡散 |
| M/S | dry_width | 0.3 | 原音は狭く |
| | wet_width | 1.8 | 残響は広く |
| EQ | tilt | -3dB | 暗い音色 |

---

### 2.5 シナリオ詳細定義

#### 2.5.1 forest（森林）

```yaml
forest:
  description: "木々に囲まれた森林環境"
  propagation:
    model: iso_9613_simplified
    humidity: 70%
    absorption_coefficient: 0.15  # 葉による散乱
  reverb:
    rt60: 0.8s
    pre_delay: 15ms
    early_reflections:
      density: high
      pattern: scattered  # 不規則な反射
    damping: 0.5
  eq:
    high_shelf: -2dB @ 8kHz  # 葉による高域吸収
    low_shelf: +1dB @ 200Hz  # 地面の反射
  spatial:
    width: 1.2
    decorrelation: 0.4
```

#### 2.5.2 cave（洞窟）

```yaml
cave:
  description: "石灰岩の洞窟、長いフラッターエコー"
  propagation:
    model: geometric
    reflection_coefficient: 0.85  # 硬い壁面
  reverb:
    rt60: 4.0s
    pre_delay: 30ms
    early_reflections:
      density: medium
      pattern: flutter  # 平行壁によるフラッター
      flutter_rate: 15Hz
    damping: 0.3  # 低ダンピング（明るい残響）
    modulation: 0.2  # 金属感軽減
  eq:
    resonance: [250Hz, 500Hz, 1kHz]  # 洞窟の共鳴
  spatial:
    width: 1.5
```

#### 2.5.3 cathedral（大聖堂）

```yaml
cathedral:
  description: "高い天井と石壁を持つ大聖堂"
  reverb:
    rt60: 6.0s
    pre_delay: 50ms
    early_reflections:
      density: medium
      pattern: geometric  # 規則的な反射
    damping: 0.4
    diffusion: 0.9  # 非常に高い拡散
  eq:
    tilt: -1dB  # やや暗め
    low_cut: 40Hz  # 超低域カット
  spatial:
    width: 1.6
    elevation_spread: 0.5  # 上方向への広がり
```

#### 2.5.4 tension（緊張）

```yaml
tension:
  description: "不安と緊張感を与える音響処理"
  eq:
    high_shelf: +3dB @ 4kHz  # 高域強調
    peak: +4dB @ 2.5kHz, Q=3  # 不快な帯域強調
  dynamics:
    compression_ratio: 4:1
    attack: 5ms  # 速いアタック
    release: 50ms
  modulation:
    pitch_wobble: 0.5%  # 微細なピッチ揺れ
    rate: 0.3Hz  # 非常にゆっくり
  reverb:
    rt60: 1.2s
    damping: 0.2  # 明るく鋭い残響
  spatial:
    width: 0.6  # 狭く閉塞感
```

#### 2.5.5 nostalgia（ノスタルジア）

```yaml
nostalgia:
  description: "温かく懐かしい音響処理"
  eq:
    high_cut: 12kHz  # 高域ロールオフ
    low_shelf: +2dB @ 300Hz  # 温かみ
    tilt: -2dB
  lofi:
    bit_depth: 12  # ビット圧縮
    sample_rate_reduction: 0.9  # 軽いエイリアシング
    wow_flutter: 0.3%  # テープ風揺れ
    noise: -40dB  # ヒスノイズ
  saturation:
    type: tape
    amount: 0.3
  reverb:
    rt60: 1.5s
    damping: 0.6
```

---

### 2.6 このアーキテクチャから導かれる必須モジュール

1. **HRTF Spatializer** - 3D定位 + 補間
2. **Physical Propagation** - 複数の吸収モデル（海水、大気、幾何）
3. **FDN Reverb** - 可変RT60、フラッターエコー対応
4. **Early Reflections** - パターン選択可能（scattered, flutter, geometric）
5. **M/S Processor** - 空間幅制御
6. **Multiband Compressor** - サイドチェイン対応
7. **Parametric EQ** - 共鳴・ピーク対応
8. **Lo-Fi Processor** - ビット圧縮、ワウフラッター
9. **Saturation** - テープ/チューブ風
10. **De-esser, Tilt EQ, Limiter** - マスタリング
11. **Scenario Blender** - シナリオの動的ブレンド

---

## 3. 機能要件

### 3.1 音響伝播モジュール (Physical Propagation)

#### 3.1.1 周波数依存吸収 (Frequency-Dependent Absorption)

**Francois-Garrison式**に基づく海水中の音波吸収をシミュレート。

```
α(f) = A₁·f₁·f² / (f₁² + f²) + A₂·f₂·f² / (f₂² + f²) + A₃·f²

ここで:
  α: 吸収係数 (dB/km)
  f: 周波数 (kHz)
  f₁, f₂: 緩和周波数 (温度・圧力依存)
  A₁, A₂, A₃: 定数 (塩分・温度・圧力依存)
```

| パラメータ | 説明 | デフォルト値 |
|-----------|------|-------------|
| `distance` | 音源からの距離 (m) | 10.0 |
| `temperature` | 水温 (°C) | 10.0 |
| `salinity` | 塩分濃度 (ppt) | 35.0 |
| `depth` | 水深 (m) | 100.0 |

#### 3.1.2 距離減衰 (Distance Attenuation)

```
L(r) = L₀ - 20·log₁₀(r/r₀) - α·(r - r₀)

ここで:
  L: 音圧レベル (dB)
  r: 距離 (m)
  r₀: 基準距離 (通常1m)
  α: 吸収係数
```

#### 3.1.3 空気中の伝播 (ISO 9613-1)

屋外環境での大気吸収。**ISO 9613-1:1993**に基づく。

##### 完全式 (参考)

```
α = 8.686 × f² × [
    1.84×10⁻¹¹ × (pa/pr)⁻¹ × (T/Tr)^0.5
  + (T/Tr)^(-2.5) × (
        0.01275 × exp(-2239.1/T) / (frO + f²/frO)
      + 0.1068 × exp(-3352/T) / (frN + f²/frN)
    )
]

ここで:
  α: 吸収係数 (dB/m)
  f: 周波数 (Hz)
  T: 絶対温度 (K)
  Tr: 基準温度 (293.15 K)
  pa: 大気圧 (kPa)
  pr: 基準圧力 (101.325 kPa)
  frO: 酸素の緩和周波数 (Hz)
  frN: 窒素の緩和周波数 (Hz)

緩和周波数:
  frO = (pa/pr) × (24 + 4.04×10⁴ × h × (0.02 + h)/(0.391 + h))
  frN = (pa/pr) × (T/Tr)^(-0.5) × (9 + 280 × h × exp(-4.170 × ((T/Tr)^(-1/3) - 1)))
  h: モル濃度比 (相対湿度から計算)
```

##### 簡略化実装

本ライブラリでは、計算コストを考慮し以下の近似式を使用:

```
α_approx(f, T, RH) = α_table[f_idx][T_idx][RH_idx]

事前計算されたルックアップテーブルを使用。
テーブルは ISO 9613-1 の完全式から生成。
```

| 周波数 | 20°C, 50%RH | 20°C, 80%RH | 10°C, 50%RH |
|-------|-------------|-------------|-------------|
| 1 kHz | 0.005 dB/m | 0.004 dB/m | 0.006 dB/m |
| 4 kHz | 0.015 dB/m | 0.011 dB/m | 0.018 dB/m |
| 8 kHz | 0.040 dB/m | 0.028 dB/m | 0.050 dB/m |

> **注**: 本実装は芸術的用途を想定した簡略化モデルです。
> 厳密な音響伝播シミュレーションには ISO 9613-1:1993 原著を参照してください。

| パラメータ | 説明 | デフォルト値 | 範囲 |
|-----------|------|-------------|------|
| `distance` | 音源からの距離 (m) | 10.0 | 0.1 - 1000 |
| `temperature` | 気温 (°C) | 20.0 | -20 - 50 |
| `humidity` | 相対湿度 (%) | 50.0 | 10 - 100 |

---

#### 3.1.4 洞窟音響モデル (Cave Acoustics)

洞窟・トンネル特有の音響現象を物理ベースでシミュレート。

##### 3.1.4.1 モード共鳴 (Modal Resonance)

洞窟の形状に基づく低周波共鳴。

**一次元近似式（本実装で使用）**:

```
f_n = n × c / (2 × L)

ここで:
  f_n: n次モードの共鳴周波数 (Hz)
  c: 音速 (343 m/s @ 20°C)
  L: 洞窟の主要寸法 (m)
  n: モード番号 (1, 2, 3...)

例: 幅15mの洞窟
  f₁ = 343 / (2 × 15) = 11.4 Hz
  f₂ = 22.9 Hz
  f₃ = 34.3 Hz
```

**三次元矩形空間の完全式（参考）**:

```
f_{n,m,l} = (c/2) × √((n/Lx)² + (m/Ly)² + (l/Lz)²)

ここで:
  Lx, Ly, Lz: 各軸方向の寸法 (m)
  n, m, l: 各軸のモード番号 (0, 1, 2...)
```

> **注**: 実際の洞窟は不規則形状のため、上記は近似です。
> 本実装では計算効率を優先し、一次元モデルを使用します。
> 知覚的には低周波の「ブーミング」感を再現することが目的です。

**実装**: 計算された共鳴周波数にEQピーク (+3〜6dB, Q=2〜4) を追加

| パラメータ | 説明 | デフォルト値 |
|-----------|------|-------------|
| `cave_dimension` | 主要寸法 (m) | 15.0 |
| `resonance_boost_db` | 共鳴ブースト量 (dB) | 4.0 |
| `resonance_modes` | 生成するモード数 | 3 |

##### 3.1.4.2 フラッターエコー (Flutter Echo)

平行な壁面による繰り返し反射:

```
T_flutter = 2 × d / c
f_flutter = c / (2 × d)

ここで:
  T_flutter: フラッター周期 (s)
  f_flutter: フラッター周波数 (Hz)
  d: 壁間距離 (m)
  c: 音速 (343 m/s)

例: 壁間8mの通路
  T_flutter = 2 × 8 / 343 = 46.6 ms
  f_flutter = 343 / (2 × 8) = 21.4 Hz (反復レート)
```

**実装**: マルチタップディレイ (46.6ms間隔で減衰しながら反復)

| パラメータ | 説明 | デフォルト値 |
|-----------|------|-------------|
| `wall_distance` | 壁間距離 (m) | 8.0 |
| `flutter_repeats` | 反復回数 | 6 |
| `flutter_decay` | 各反復の減衰率 (0-1) | 0.7 |

##### 3.1.4.3 Eyring残響時間

吸音率が低い空間（洞窟）に適したRT60計算:

```
RT60 = 0.161 × V / (-S × ln(1 - ᾱ))

ここで:
  V: 空間の体積 (m³)
  S: 表面積 (m²)
  ᾱ: 平均吸音率

Sabine式との比較 (高反射空間で重要):
  Sabine: RT60 = 0.161 × V / (S × ᾱ)
  → 吸音率が低いとき (ᾱ < 0.2) Sabineは過大評価
```

**用途**: room_size から RT60 への変換に使用

##### 3.1.4.4 岩壁の周波数依存吸音

石灰岩・花崗岩等の吸音特性:

```
α(f) = α_low + (α_high - α_low) × (log₁₀(f / f_low) / log₁₀(f_high / f_low))

典型的な岩壁 (石灰岩):
  125 Hz: α = 0.02
  500 Hz: α = 0.04
  2kHz:   α = 0.06
  8kHz:   α = 0.10

→ 低域はほぼ全反射、高域はやや吸収
→ 明るく長い残響の原因
```

| パラメータ | 説明 | デフォルト値 |
|-----------|------|-------------|
| `wall_material` | 材質 ("limestone", "granite", "concrete") | "limestone" |
| `alpha_low` | 低周波吸音率 | 0.02 |
| `alpha_high` | 高周波吸音率 | 0.10 |

##### 3.1.4.5 洞窟モデルの統合

```c
// 洞窟音響パラメータ
typedef struct {
    float cave_dimension_m;      // 主要寸法
    float wall_distance_m;       // 壁間距離 (フラッター用)
    uint8_t flutter_repeats;     // フラッター反復回数
    float flutter_decay;         // フラッター減衰
    float alpha_low;             // 低周波吸音率
    float alpha_high;            // 高周波吸音率
} ae_cave_params_t;

// 洞窟モデル適用
ae_result_t ae_apply_cave_model(
    ae_engine_t* engine,
    const ae_cave_params_t* params
);
```

---

### 3.2 空間音響モジュール (Spatial Audio)

#### 3.2.1 HRTF Spatializer

| 機能 | 詳細 |
|-----|------|
| 対応フォーマット | SOFA (Spatially Oriented Format for Acoustics) |
| デフォルトHRTF | MIT KEMAR (anechoic) |
| 方向補間 | 球面線形補間 (SLERP) |
| 切り替え | クロスフェード (10-50ms, 設定可能) |
| 畳み込み | パーティション畳み込み (低レイテンシ) |

#### 3.2.2 M/S Processor

```
Mid   = (L + R) / 2
Side  = (L - R) / 2

L_out = Mid + Side·width
R_out = Mid - Side·width
```

| パラメータ | 範囲 | 説明 |
|-----------|------|------|
| `width` | 0.0 - 2.0 | 0=モノラル, 1=ステレオ, 2=超ワイド |
| `mid_gain` | -∞ - +12dB | Mid成分ゲイン |
| `side_gain` | -∞ - +12dB | Side成分ゲイン |

#### 3.2.3 デコリレーション

IACC (Interaural Cross-Correlation) を低減して空間的広がりを増す。

- オールパスフィルタによる位相分散
- 微小遅延差の付加
- ピッチシフトなしのステレオワイドニング

---

### 3.3 リバーブモジュール (Reverb)

#### 3.3.1 FDN (Feedback Delay Network)

| パラメータ | 範囲 | 説明 |
|-----------|------|------|
| `rt60` | 0.1 - 30.0 sec | 残響時間 |
| `pre_delay` | 0 - 500 ms | 初期反射までの遅延 |
| `damping` | 0.0 - 1.0 | 高域減衰率 |
| `size` | 0.0 - 1.0 | 空間サイズ感 |
| `diffusion` | 0.0 - 1.0 | 拡散度 |
| `modulation` | 0.0 - 1.0 | 内部変調 (金属感抑制) |

**FDN構成**:
- 8チャンネル (標準) / 16チャンネル (高品質)
- Hadamard行列によるフィードバック
- 周波数依存減衰 (per-band RT60)

#### 3.3.2 Early Reflections

- 最大16タップのマルチタップディレイ
- ルームシェイプに基づくプリセット
- レイトレースベースのカスタム設定 (将来)

---

### 3.4 ダイナミクスモジュール (Dynamics)

#### 3.4.1 コンプレッサー

| パラメータ | 範囲 | 説明 |
|-----------|------|------|
| `threshold` | -60 - 0 dB | しきい値 |
| `ratio` | 1:1 - 20:1 | 圧縮比 |
| `attack` | 0.1 - 100 ms | アタック時間 |
| `release` | 10 - 1000 ms | リリース時間 |
| `knee` | 0 - 20 dB | ソフトニー幅 |
| `makeup` | 0 - 24 dB | メイクアップゲイン |

**追加機能**:
- サイドチェイン入力
- 外部キー/内部キー切り替え
- マルチバンド (3-5バンド)

#### 3.4.2 ルックアヘッドリミッター

- ルックアヘッド: 1-10ms (設定可能)
- トゥルーピーク検出
- ソフトクリップ / ハードクリップ選択

#### 3.4.3 デエッサー

- 周波数帯域: 4-10kHz (自動検出またはマニュアル)
- ワイドバンド / スプリットバンドモード

---

### 3.5 トーン/EQモジュール (Tone/EQ)

#### 3.5.1 Tilt EQ

```
      ↑ Gain
      │      ___________
      │     /
      │    /
------│---●-------------- Pivot (1kHz default)
      │  /
      │ /
      │/___________
      └─────────────────▶ Frequency
```

| パラメータ | 範囲 | 説明 |
|-----------|------|------|
| `tilt` | -6 - +6 dB | 傾斜量 |
| `pivot` | 200 - 4000 Hz | ピボット周波数 |

#### 3.5.2 パラメトリックEQ

- 最大8バンド
- Peak / Low Shelf / High Shelf / Notch
- Q: 0.1 - 30

---

### 3.6 生体信号マッピング (Biosignal Mapping)

#### 3.6.1 対応信号

| 信号 | 単位 | 典型的な範囲 |
|-----|------|-------------|
| Heart Rate (HR) | BPM | 40 - 200 |
| Heart Rate Variability (HRV) | ms (RMSSD) | 10 - 100 |

#### 3.6.2 マッピング定義

```c
typedef struct {
    ae_biosignal_type_t input;      // 入力信号種別
    ae_param_target_t   target;     // 出力パラメータ
    ae_curve_type_t     curve;      // マッピングカーブ
    float               in_min;     // 入力範囲最小
    float               in_max;     // 入力範囲最大
    float               out_min;    // 出力範囲最小
    float               out_max;    // 出力範囲最大
    float               smoothing;  // スムージング時定数 (ms)
} ae_mapping_t;
```

#### 3.6.3 マッピングカーブ

| カーブ | 用途 |
|-------|------|
| `LINEAR` | 比例関係 |
| `EXPONENTIAL` | 知覚に近い（周波数、ゲイン） |
| `LOGARITHMIC` | 圧縮関係 |
| `SIGMOID` | 柔らかい遷移、飽和あり |
| `STEPPED` | 段階的変化 |
| `CUSTOM` | ユーザー定義ルックアップテーブル |

---

### 3.7 セマンティック表現 (Semantic Expressions)

#### 3.7.1 Expression文法

```
expression := param_name ":" value ("," param_name ":" value)*
value      := float (0.0 - 1.0)
```

#### 3.7.2 対応Expression

| Expression | 内部マッピング |
|------------|---------------|
| `distance` | Dry/Wet比, HPF, 吸収 |
| `depth` | Francois-Garrison深度, リバーブ |
| `room_size` | RT60, ER密度 |
| `tension` | HPF, 軽い歪み, コンプ |
| `warmth` | ローシェルフ, サチュレーション |
| `intimacy` | 近接効果, Dry優位 |
| `chaos` | 変調深度, 非同期ディレイ |
| `underwater` | Francois-Garrison + LPF + 変調 |

#### 3.7.3 SLMによる自然言語インターフェース (将来拡張)

**目的**: ユーザーが自然言語で「欲しい音色」を記述し、SLM (Small Language Model) がそれを物理パラメータに変換する。

##### 設計思想

```
┌─────────────────────────────────────────────────────────────────┐
│                 Natural Language → Physical Parameters          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  User Input (自然言語)                                          │
│  「深い水中にいるような、冷たくて暗い音にしたい」                  │
│  "I want it to sound like being deep underwater, cold and dark" │
│                          │                                      │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                   SLM (Small Language Model)              │ │
│  │                                                           │ │
│  │  入力: 自然言語記述                                        │ │
│  │  出力: JSON形式のパラメータセット                          │ │
│  │                                                           │ │
│  │  推奨モデル:                                               │ │
│  │  - Llama-3.2-1B / 3B (ローカル実行)                       │ │
│  │  - Phi-3-mini (軽量、エッジ向け)                          │ │
│  │  - Gemma-2B (オープンソース)                              │ │
│  └───────────────────────────────────────────────────────────┘ │
│                          │                                      │
│                          ▼                                      │
│  Structured Output (JSON)                                       │
│  {                                                              │
│    "preset": "deep_sea",                                        │
│    "params": {                                                  │
│      "brightness": -0.7,                                        │
│      "room_size": 0.9,                                          │
│      "distance": 80.0                                           │
│    },                                                           │
│    "emotion": {                                                 │
│      "coldness": 0.8,                                           │
│      "darkness": 0.7                                            │
│    }                                                            │
│  }                                                              │
│                          │                                      │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │              Parameter Resolver (既存モジュール)           │ │
│  │  JSON → ae_main_params_t → 内部パラメータ                 │ │
│  └───────────────────────────────────────────────────────────┘ │
│                          │                                      │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │              Audio Processing Pipeline                    │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

##### SLMプロンプト設計

```
System Prompt:
あなたは音響エンジニアです。ユーザーの音の印象に関する記述を、
以下のパラメータに変換してください。

利用可能なパラメータ:
- distance (0.1-1000): 音源距離 (m)
- room_size (0-1): 空間の大きさ感
- brightness (-1 to 1): 音の明るさ
- width (0-2): 空間的広がり
- dry_wet (0-1): 原音とエフェクトの比率
- intensity (0-1): 効果の強さ

利用可能なプリセット:
deep_sea, cave, forest, cathedral, tension, nostalgia, intimate, ...

感情/印象キーワード:
coldness, warmth, darkness, brightness, intimacy, vastness, 
eeriness, comfort, anxiety, nostalgia, ...

出力形式: 厳密にJSONのみ
```

##### C API

```c
// SLM統合 (オプションモジュール)
typedef struct {
    const char* model_path;      // ONNXモデルパス
    size_t max_tokens;           // 最大トークン数
    float temperature;           // サンプリング温度
} ae_slm_config_t;

// SLMエンジンの初期化
ae_slm_t* ae_slm_create(const ae_slm_config_t* config);
void ae_slm_destroy(ae_slm_t* slm);

// 自然言語からパラメータへ変換
ae_result_t ae_slm_interpret(
    ae_slm_t* slm,
    const char* natural_language_input,  // "深い水中のような音"
    ae_main_params_t* params_out,        // 出力パラメータ
    char* preset_name_out,               // 推奨プリセット名
    size_t preset_name_size
);

// 使用例
ae_slm_t* slm = ae_slm_create(&slm_config);
ae_main_params_t params;
char preset[64];

ae_slm_interpret(slm, "薄暗い洞窟の中で響くような音にしたい", 
                 &params, preset, sizeof(preset));

// params.room_size ≈ 0.7, params.brightness ≈ -0.2
// preset = "cave"

ae_load_preset(engine, preset);
ae_set_main_params(engine, &params);
```

##### 知覚キーワード → 物理パラメータ マッピング

SLMが学習すべきマッピング関係:

| 知覚キーワード | 影響するパラメータ | 変換ロジック |
|--------------|-----------------|------------|
| "深い" / "deep" | room_size ↑, distance ↑ | 空間サイズと距離を増加 |
| "冷たい" / "cold" | brightness ↓, warmth ↓ | 高域減衰、低域抑制 |
| "暗い" / "dark" | brightness ↓↓ | 強い高域減衰 |
| "広い" / "wide" | width ↑, room_size ↑ | ステレオ幅と空間拡大 |
| "近い" / "close" | distance ↓, dry_wet ↓ | 近接感、ドライ優位 |
| "不安" / "anxious" | tension ↑ | tension プリセット方向 |
| "懐かしい" / "nostalgic" | lofi_amount ↑ | nostalgia プリセット方向 |
| "神秘的" / "ethereal" | diffusion ↑, modulation ↑ | 拡散と変調を増加 |

##### フィードバックループ

```
┌────────────────────────────────────────────────────────────────┐
│                    Iterative Refinement                        │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  1. User: 「もう少し暗くして」                                  │
│     SLM: brightness を -0.2 減少                               │
│                                                                │
│  2. User: 「残響を短くして」                                    │
│     SLM: room_size を -0.15 減少                               │
│                                                                │
│  3. User: 「いい感じ！これを保存して」                          │
│     → カスタムプリセットとして保存                              │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

##### 実装優先度

| 機能 | 優先度 | フェーズ |
|-----|-------|---------|
| 基本的なキーワード→パラメータ変換 | 高 | Phase 2 |
| プリセット選択サジェスト | 高 | Phase 2 |
| 差分調整 ("もう少し〜") | 中 | Phase 3 |
| 対話的リファインメント | 低 | Phase 4 |
| オンデバイスSLM推論 | 低 | 将来 |

---

### 3.8 知覚心理学的基盤 (Psychoacoustic Foundation)

本ライブラリのパラメータ設計は、以下の知覚心理学研究に基づいています。

#### 3.8.1 距離知覚 (Distance Perception)

聴覚的距離知覚は複数の手がかりに依存する (Zahorik, 2002):

| 手がかり | 物理量 | 本ライブラリでの実装 |
|---------|-------|---------------------|
| **音圧レベル** | 逆二乗則による減衰 | distance → gain |
| **直接音/残響比 (DRR)** | direct / reverberant | distance → dry_wet |
| **スペクトル変化** | 高周波吸収 | distance → HF attenuation |
| **両耳手がかり** | ILD減少 | distance → width reduction |

```c
// 距離から知覚パラメータへの変換
void distance_to_perceptual(float distance_m, ae_perceptual_t* out) {
    // Zahorik (2002): 知覚距離 ≈ 実距離^0.54 (圧縮関係)
    float perceptual_distance = powf(distance_m, 0.54f);
    
    // DRR: 残響半径 (約10m) を基準に計算
    // Bronkhorst & Houtgast (1999)
    float reverb_radius = 10.0f;  // 典型的な室内
    out->drr_db = 20.0f * log10f(reverb_radius / distance_m);
    
    // 逆二乗則による減衰
    out->level_db = -20.0f * log10f(distance_m);
}
```

#### 3.8.2 空間知覚 (Spatial Perception)

**IACC (Interaural Cross-Correlation)** は空間の広がり感の主要指標:

```
IACC = max(|∫ pL(t) × pR(t+τ) dt| / √(∫pL²dt × ∫pR²dt))

ここで:
  pL, pR: 左右耳の音圧信号
  τ: 時間ずれ (-1ms 〜 +1ms)
  IACC: 0 (完全非相関) 〜 1 (完全相関)
```

| IACC値 | 知覚 | 本ライブラリのwidthパラメータ |
|-------|------|---------------------------|
| 0.9-1.0 | 非常に狭い、モノラル的 | width = 0.0-0.3 |
| 0.6-0.9 | 通常のステレオ | width = 0.5-1.0 |
| 0.3-0.6 | 広い、包囲感あり | width = 1.0-1.5 |
| 0.0-0.3 | 非常に広い、拡散的 | width = 1.5-2.0 |

**ASW (Apparent Source Width)** と **LEV (Listener Envelopment)**:

```
ASW: 見かけの音源幅 → 初期反射 (0-80ms) のIACCに依存
LEV: 聴取者包囲感 → 後期残響 (80ms以降) のIACCに依存

Beranek (2004): 
  - IACCE (Early) < 0.5 で良好なASW
  - IACCL (Late) < 0.3 で良好なLEV
```

#### 3.8.3 ラウドネス知覚 (Loudness Perception)

**等ラウドネス曲線 (ISO 226:2003)** に基づく周波数重み付け:

```
Fletcher-Munson曲線の影響:
  - 人間の耳は 2-5 kHz に最も敏感
  - 低音量では低周波感度が大幅に低下
  - brightness パラメータはこれを考慮

Phon → Sone 変換 (Stevens, 1957):
  S = 2^((P - 40) / 10)
  
  ここで:
    S: ラウドネス (sone)
    P: ラウドネスレベル (phon)
```

#### 3.8.4 パラメータの知覚的スケーリング

本ライブラリの各パラメータは知覚的に線形になるよう設計:

| パラメータ | 物理スケール | 知覚スケール | 変換式 |
|-----------|------------|------------|--------|
| `brightness` | -1.0 〜 1.0 (線形) | 知覚的に均等 | tilt_db = brightness × 6.0 |
| `room_size` | 0.0 〜 1.0 (線形) | 部屋サイズ感 | RT60 = 0.1 × exp(room_size × 3.4) |
| `distance` | 0.1 〜 1000 m (対数) | 距離感 | 対数スケール推奨 |
| `dry_wet` | 0.0 〜 1.0 (線形) | 近さ/遠さ | DRRに対応 |

```c
// room_size → RT60 変換 (知覚的に線形)
float room_size_to_rt60(float room_size) {
    // 0.0 → 0.1秒, 0.5 → 1.1秒, 1.0 → 30秒 (指数スケール)
    return 0.1f * expf(room_size * 3.4f);
}

// 逆変換
float rt60_to_room_size(float rt60) {
    return logf(rt60 / 0.1f) / 3.4f;
}
```

#### 3.8.5 感情・印象パラメータの心理学的根拠

| パラメータ | 心理学的根拠 | 参考研究 |
|-----------|------------|---------|
| `tension` | 高周波強調は覚醒度を高める | Västfjäll (2012) |
| `warmth` | 低周波/中周波強調は温かみ | Juslin & Laukka (2004) |
| `intimacy` | 低いDRR、近接効果 | Griesinger (1997) |
| `chaos` | 予測不能な変調は不安を誘発 | Huron (2006) |

#### 3.8.6 知覚評価指標

処理結果の知覚品質を評価するための指標:

```c
typedef struct {
    float iacc_early;        // 初期IACC (0-80ms) → ASW
    float iacc_late;         // 後期IACC (80ms-) → LEV
    float drr_db;            // 直接音/残響比 (dB)
    float spectral_centroid; // スペクトル重心 (Hz) → brightness
    float loudness_sone;     // ラウドネス (sone)
    float clarity_c50;       // 明瞭度 C50 (dB)
    float clarity_c80;       // 明瞭度 C80 (dB)
} ae_perceptual_metrics_t;

// 知覚指標の計算
ae_result_t ae_compute_perceptual_metrics(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_perceptual_metrics_t* out
);
```

#### 3.8.7 Zwicker/Fastlモデルに基づく知覚パラメータ

音質評価の標準的フレームワーク (Zwicker & Fastl, 1999) に基づく追加パラメータ:

##### Roughness (粗さ)

```
単位: asper (1 asper = 60dB, 1kHz, 100%AM, 70Hz変調)

定義: 15-300Hzの振幅変調による「ザラザラ感」
最大知覚: 変調周波数 ≈ 70Hz

R = ∫ (dL/dt)² × f_mod × g(f_carrier) df

ここで:
  dL/dt: ラウドネスの時間変化率
  f_mod: 変調周波数
  g(): キャリア周波数による重み付け
```

| roughness値 | 知覚 | 用途例 |
|-------------|------|-------|
| 0.0-0.5 asper | 滑らか | 通常の音声、音楽 |
| 0.5-1.5 asper | やや粗い | ディストーションギター |
| 1.5-3.0 asper | 粗い | グランジ、不快感表現 |
| > 3.0 asper | 非常に粗い | ノイズ、警告音 |

##### Sharpness (鋭さ)

```
単位: acum (1 acum = 60dB, 1kHz, 1/3オクターブノイズ)

定義: 高周波成分の重み付けによる「刺すような」感覚

S = 0.11 × ∫ N'(z) × g(z) × z dz / ∫ N'(z) dz

ここで:
  N'(z): 特定ラウドネス (Bark帯域ごと)
  g(z): 高域強調の重み関数 (z > 15 Bark で増加)
  z: 臨界帯域率 (Bark)
```

| sharpness値 | 知覚 | brightness との関係 |
|-------------|------|-------------------|
| < 1.0 acum | 丸い、温かい | brightness ≈ -0.5 |
| 1.0-1.5 acum | 中性 | brightness ≈ 0 |
| 1.5-2.5 acum | 明るい、鋭い | brightness ≈ 0.5 |
| > 2.5 acum | 刺激的、攻撃的 | brightness ≈ 1.0 |

##### Fluctuation Strength (変動強度)

```
単位: vacil (1 vacil = 60dB, 1kHz, 100%AM, 4Hz変調)

定義: 20Hz未満の緩やかな振幅変調による「うねり」感
最大知覚: 変調周波数 ≈ 4Hz (音節レートに近い)

F = 0.008 × ∫ (dL/dt)² / ((f_mod/4) + (4/f_mod)) df
```

| fluctuation値 | 知覚 | 用途例 |
|--------------|------|-------|
| 0.0-0.5 vacil | 安定 | 通常の音 |
| 0.5-1.5 vacil | 軽い揺らぎ | トレモロ、ビブラート |
| 1.5-3.0 vacil | 顕著な変動 | サイレン、アラーム |
| > 3.0 vacil | 強い変動 | 不安感、警告表現 |

##### Tonality (調性)

```
定義: 音がトーン的 (周期的) かノイズ的 (非周期的) かの度合い

T = Σ (tonal_loudness) / (Σ tonal_loudness + Σ noise_loudness)

ここで:
  tonal_loudness: 明確なピークを持つ成分のラウドネス
  noise_loudness: 広帯域成分のラウドネス
  T: 0 (完全ノイズ) 〜 1 (完全トーン)
```

| tonality値 | 知覚 | 例 |
|-----------|------|---|
| 0.0-0.3 | ノイズ的 | ホワイトノイズ、風音 |
| 0.3-0.7 | 混合 | 音声、雑踏 |
| 0.7-1.0 | トーン的 | 楽器音、電子音 |

#### 3.8.8 音色記述パラメータ (Timbral Descriptors)

音楽・音響工学で一般的に使用される知覚的音色記述:

| パラメータ | 知覚的定義 | 物理的相関 | 本ライブラリでの実装 |
|-----------|----------|----------|-------------------|
| **Warmth** | 温かみ、柔らかさ | 200-500Hz強調、高域減衰 | brightness ↓, low_shelf ↑ |
| **Presence** | 存在感、近さ | 2-5kHz強調 | mid_boost (2-5kHz) |
| **Air** | 空気感、透明感 | 10kHz以上の成分 | high_shelf ↑ (>10kHz) |
| **Body/Fullness** | 厚み、豊かさ | 100-300Hz強調 | low_mid_boost |
| **Clarity** | 明瞭さ | 高DRR、低残響 | dry_wet ↓, room_size ↓ |
| **Punch** | アタック感 | 速いトランジェント | attack_enhance |
| **Smoothness** | 滑らかさ | 低roughness | roughness ↓ |

#### 3.8.9 拡張パラメータへのマッピング

現在の10パラメータと知覚パラメータの対応:

```
┌────────────────────────────────────────────────────────────────┐
│           知覚パラメータ → 処理パラメータ マッピング             │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Sharpness (acum)    ←→  brightness, EQ high shelf           │
│  Roughness (asper)   ←→  distortion, modulation depth        │
│  Fluctuation (vacil) ←→  tremolo rate/depth, LFO             │
│  Tonality            ←→  noise gate, harmonic enhancement    │
│                                                                │
│  Warmth              ←→  brightness ↓ + low shelf ↑          │
│  Presence            ←→  mid boost (2-5kHz)                  │
│  Air                 ←→  high shelf (>10kHz)                 │
│  Clarity             ←→  dry_wet ↓ + compression             │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

##### C API拡張

```c
// 知覚パラメータ構造体 (拡張)
typedef struct {
    // Zwicker/Fastl パラメータ
    float roughness;           // 0.0 - 5.0 asper
    float sharpness;           // 0.0 - 4.0 acum
    float fluctuation;         // 0.0 - 5.0 vacil
    float tonality;            // 0.0 - 1.0
    
    // 音色記述パラメータ
    float warmth;              // 0.0 - 1.0
    float presence;            // 0.0 - 1.0
    float air;                 // 0.0 - 1.0
    float body;                // 0.0 - 1.0
    float clarity;             // 0.0 - 1.0
    float punch;               // 0.0 - 1.0
} ae_timbral_params_t;

// 知覚パラメータから処理パラメータへの変換
ae_result_t ae_timbral_to_processing(
    const ae_timbral_params_t* timbral,
    ae_main_params_t* main_out,
    ae_extended_params_t* extended_out
);

// 処理結果から知覚パラメータを分析
ae_result_t ae_analyze_timbral(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_timbral_params_t* out
);

// 使用例: 「温かく滑らかな音にしたい」
ae_timbral_params_t timbral = {
    .warmth = 0.8f,
    .roughness = 0.1f,
    .sharpness = 0.5f,
    .clarity = 0.6f
};
ae_timbral_to_processing(&timbral, &main_params, &ext_params);
ae_set_main_params(engine, &main_params);
```

---

### 3.9 追加音響パラメータ (Extended Acoustic Parameters)

#### 3.9.1 ISO 3382 室内音響指標

ISO 3382-1:2009 に基づく室内音響評価指標。

##### EDT (Early Decay Time)

```
定義: インパルス応答の最初の10dB減衰から推定される残響時間

EDT = 6 × t₁₀

ここで:
  t₁₀: 0dB から -10dB に減衰するまでの時間 (秒)
  
RT60との関係:
  - 拡散音場では EDT ≈ RT60
  - 直接音・初期反射が強い場合 EDT < RT60
  - 人間の残響感知覚は EDT により近い

測定帯域: 125Hz - 4kHz (オクターブバンド)
```

| EDT/RT60 比 | 知覚 | 空間特性 |
|------------|------|---------|
| < 0.8 | 明瞭、ドライ | 直接音優位 |
| 0.8-1.0 | バランス良好 | 適度な拡散 |
| > 1.0 | 残響的、ウェット | 後期残響優位 |

##### C50 (Clarity for Speech)

```
C₅₀ = 10 × log₁₀(∫₀⁵⁰ᵐˢ p²(t)dt / ∫₅₀ᵐˢ^∞ p²(t)dt)  [dB]

ここで:
  p(t): インパルス応答の音圧
  50ms: 音声の子音知覚に重要な境界時間

解釈:
  C₅₀ > 0 dB: 早期エネルギー優位 → 良好な明瞭度
  C₅₀ < 0 dB: 後期エネルギー優位 → 明瞭度低下
```

| C50 値 | 評価 | 用途 |
|-------|------|------|
| > +6 dB | 非常に明瞭 | 講義室、会議室 |
| +2 〜 +6 dB | 良好 | 一般的なスピーチ |
| -2 〜 +2 dB | 許容範囲 | 劇場 |
| < -2 dB | 不明瞭 | 過度な残響 |

##### C80 (Clarity for Music)

```
C₈₀ = 10 × log₁₀(∫₀⁸⁰ᵐˢ p²(t)dt / ∫₈₀ᵐˢ^∞ p²(t)dt)  [dB]

音楽では80msまでの反射が「定義感」に寄与

理想範囲 (Beranek):
  シンフォニー: -2 〜 +2 dB
  室内楽: +1 〜 +4 dB
  オペラ: +2 〜 +5 dB
```

| C80 値 | 評価 | 音楽ジャンル |
|-------|------|------------|
| > +4 dB | 非常にクリア | ポップス、スピーチ |
| +1 〜 +4 dB | クリア | 室内楽 |
| -2 〜 +1 dB | 温かい | シンフォニー |
| < -2 dB | ぼやける | ロマン派向き |

##### D50 (Definition / Deutlichkeit)

```
D₅₀ = ∫₀⁵⁰ᵐˢ p²(t)dt / ∫₀^∞ p²(t)dt  [0-1]

C50 との関係:
  C₅₀ = 10 × log₁₀(D₅₀ / (1 - D₅₀))

一般に D₅₀ > 0.5 (50%) で良好な明瞭度
```

##### Ts (Center Time / Schwerpunktzeit)

```
Tₛ = ∫₀^∞ t × p²(t)dt / ∫₀^∞ p²(t)dt  [秒]

定義: エネルギーの時間的重心
値が小さいほど早期エネルギー優位

典型値:
  明瞭な空間: Ts < 60ms
  残響的空間: Ts > 150ms
```

##### G (Strength / Sound Strength)

```
G = Lₚ(receiver) - Lₚ(10m, free field)  [dB]

ここで:
  Lₚ(receiver): 受音点での音圧レベル
  Lₚ(10m, free field): 同じ音源の自由音場10mでの音圧レベル

室容積との関係 (Barron & Lee):
  G ≈ 10 × log₁₀(31200 × T / V)
  
  T: RT60, V: 室容積 (m³)
```

| G 値 | 知覚 | 典型的空間 |
|-----|------|----------|
| > +10 dB | 非常に響く | 小さな教会 |
| +5 〜 +10 dB | 響く | コンサートホール |
| 0 〜 +5 dB | 適度 | 劇場 |
| < 0 dB | 弱い | 大きなアリーナ |

##### STI (Speech Transmission Index)

```
STI = Σᵢ Σⱼ wᵢⱼ × MTI(fᵢ, Fⱼ) / Σ wᵢⱼ

ここで:
  MTI: Modulation Transfer Index
  fᵢ: オクターブ帯域中心周波数 (125Hz - 8kHz)
  Fⱼ: 変調周波数 (0.63 - 12.5 Hz)
  wᵢⱼ: 重み係数

STI値 [0-1]:
  0.00-0.30: Bad (不可)
  0.30-0.45: Poor (劣る)
  0.45-0.60: Fair (可)
  0.60-0.75: Good (良好)
  0.75-1.00: Excellent (優秀)
```

##### C API: 室内音響指標

```c
typedef struct {
    float edt;              // Early Decay Time (秒)
    float edt_band[6];      // 帯域別 EDT (125Hz-4kHz)
    float c50;              // Clarity for speech (dB)
    float c80;              // Clarity for music (dB)
    float d50;              // Definition (0-1)
    float ts_ms;            // Center time (ms)
    float strength_g;       // Sound strength (dB)
    float sti;              // Speech Transmission Index (0-1)
} ae_room_metrics_t;

// インパルス応答から室内音響指標を計算
ae_result_t ae_compute_room_metrics(
    const float* impulse_response,
    size_t ir_length,
    uint32_t sample_rate,
    ae_room_metrics_t* out
);
```

---

#### 3.9.2 両耳聴パラメータ (Binaural Parameters)

##### ITD (Interaural Time Difference)

```
定義: 音が両耳に到達する時間差

ITD = (d × sin(θ)) / c

ここで:
  d: 両耳間距離 (約0.215m)
  θ: 音源方位角 (正面=0°)
  c: 音速 (343 m/s)

最大ITD (θ = 90°): 約 ±625 μs

知覚閾値: 約 10-20 μs (正面付近)
```

| ITD 値 | 知覚方向 |
|-------|---------|
| 0 μs | 正面 |
| ±300 μs | 約 45° |
| ±625 μs | 真横 (90°) |

##### ILD (Interaural Level Difference)

```
定義: 両耳間の音圧レベル差

ILD = 20 × log₁₀(pᵣ / pₗ)  [dB]

周波数依存性:
  - 低周波 (< 500Hz): ILD ≈ 0 (回折により頭を回り込む)
  - 高周波 (> 1500Hz): ILD 最大約 20dB (頭部の影)

方向知覚への寄与:
  - 低周波: 主にITDに依存
  - 高周波: 主にILDに依存
  - 交差周波数: 約 1500Hz
```

##### 先行音効果 (Precedence Effect / Haas Effect)

```
定義: 同一音が短い遅延で到達した場合、最初の音の方向に定位する現象

有効遅延範囲:
  - 1-5 ms: 音像のわずかな広がり
  - 5-30 ms: 先行音への定位 (エコー知覚なし)
  - 30-50 ms: エコー閾値 (エコーとして知覚開始)
  - > 50 ms: 明確なエコー

実装:
  初期反射のパンニングを先行音方向に寄せる
```

##### C API: 両耳聴パラメータ

```c
typedef struct {
    float itd_us;           // ITD (マイクロ秒) -625 〜 +625
    float ild_db;           // ILD (dB) -20 〜 +20
    float azimuth_deg;      // 方位角 (度) -180 〜 +180
    float elevation_deg;    // 仰角 (度) -90 〜 +90
} ae_binaural_params_t;

// 方位角からITD/ILDを計算
ae_result_t ae_azimuth_to_binaural(
    float azimuth_deg,
    float elevation_deg,
    float frequency_hz,
    ae_binaural_params_t* out
);

// 先行音効果の適用
typedef struct {
    float delay_ms;         // 遅延時間 (ms)
    float level_db;         // レベル差 (dB)
    float pan;              // パンニング (-1 〜 +1)
} ae_precedence_t;

ae_result_t ae_apply_precedence(
    ae_engine_t* engine,
    const ae_precedence_t* params
);
```

---

#### 3.9.3 動的パラメータ (Dynamic Parameters)

##### ドップラー効果 (Doppler Effect)

```
f' = f × (c ± vᵣ) / (c ∓ vₛ)

ここで:
  f': 観測周波数
  f: 音源周波数
  c: 音速 (343 m/s)
  vᵣ: 受音者速度 (近づく方向が正)
  vₛ: 音源速度 (近づく方向が正)

近似 (vₛ << c):
  Δf/f ≈ vₛ/c
  
例: 30 m/s (108 km/h) → 約 8.7% の周波数変化
```

| 速度 | 周波数変化 | 知覚 |
|-----|----------|------|
| 10 m/s | ±2.9% | わずかに知覚可能 |
| 30 m/s | ±8.7% | 明確 (サイレン等) |
| 100 m/s | ±29% | 非常に顕著 |

##### ADSR エンベロープ

```
┌─────────────────────────────────────────────────────────────┐
│                     ADSR Envelope                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Level │     /\                                             │
│   1.0  │    /  \___________________                         │
│        │   /    D                  \                        │
│   S    │  /                         \                       │
│        │ /A                       R  \                      │
│   0.0  │/_____________________________ \___                 │
│        └─────────────────────────────────────── Time        │
│        |  A  | D |        S        |  R  |                  │
│                                                             │
│  A: Attack time (0-1000 ms)                                 │
│  D: Decay time (0-2000 ms)                                  │
│  S: Sustain level (0-1)                                     │
│  R: Release time (0-5000 ms)                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

| 音色タイプ | Attack | Decay | Sustain | Release |
|-----------|--------|-------|---------|---------|
| ピアノ | 1ms | 500ms | 0.3 | 1000ms |
| パッド | 500ms | 200ms | 0.8 | 2000ms |
| パーカッション | 0ms | 50ms | 0.0 | 100ms |
| ストリングス | 200ms | 100ms | 0.9 | 500ms |

##### 時間マスキング (Temporal Masking)

```
同時マスキング: 同時に鳴る音間のマスキング
前方マスキング: 先行音による後続音のマスキング (最大200ms)
後方マスキング: 後続音による先行音のマスキング (最大20ms)

マスキング量:
  Δt = 0ms (同時): 最大マスキング
  Δt = 50ms: 約 -20dB
  Δt = 100ms: 約 -30dB
  Δt = 200ms: ほぼ0
```

##### C API: 動的パラメータ

```c
// ドップラー効果
typedef struct {
    float source_velocity;   // 音源速度 (m/s) 正=接近
    float listener_velocity; // 聴取者速度 (m/s)
    bool enabled;            // ドップラー有効
} ae_doppler_params_t;

ae_result_t ae_set_doppler(ae_engine_t* engine, const ae_doppler_params_t* params);

// ADSRエンベロープ
typedef struct {
    float attack_ms;         // アタック時間 (ms)
    float decay_ms;          // ディケイ時間 (ms)
    float sustain_level;     // サスティンレベル (0-1)
    float release_ms;        // リリース時間 (ms)
} ae_adsr_t;

ae_result_t ae_set_envelope(ae_engine_t* engine, const ae_adsr_t* envelope);
```

---

#### 3.9.4 スペクトル特性 (Spectral Characteristics)

##### スペクトル重心 (Spectral Centroid)

```
SC = Σₖ f(k) × |X(k)|² / Σₖ |X(k)|²

ここで:
  f(k): k番目のビンの周波数
  X(k): FFT係数

解釈:
  - 知覚的な「明るさ」と強い相関
  - 高いSC → 明るい音色
  - 低いSC → 暗い、温かい音色

典型値:
  バス音声: 300-500 Hz
  一般音声: 800-1500 Hz
  シンバル: 5000-10000 Hz
```

##### スペクトル拡がり (Spectral Spread)

```
SS = √(Σₖ (f(k) - SC)² × |X(k)|² / Σₖ |X(k)|²)

解釈:
  - 周波数分布の広がり
  - 大きいSS → 広帯域、豊かな倍音
  - 小さいSS → 狭帯域、純音に近い
```

##### スペクトルフラックス (Spectral Flux)

```
SF = Σₖ (|Xₙ(k)| - |Xₙ₋₁(k)|)²

定義: フレーム間のスペクトル変化量

用途:
  - 音響イベント検出
  - オンセット検出
  - 動きの激しさの指標
```

##### 調波性 (Harmonicity / HNR)

```
HNR = 10 × log₁₀(Eₕ / Eₙ)  [dB]

ここで:
  Eₕ: 調波成分のエネルギー
  Eₙ: 非調波成分 (ノイズ) のエネルギー

解釈:
  - 高いHNR → クリアな音色 (楽器、母音)
  - 低いHNR → ノイジー (摩擦音、ささやき)

典型値:
  純音: > 40 dB
  母音: 20-30 dB
  子音: < 10 dB
```

##### Bark帯域スペクトル

```
臨界帯域 (Critical Band) に基づく知覚的周波数分析

Bark番号計算:
  z = 13 × arctan(0.00076 × f) + 3.5 × arctan((f/7500)²)

帯域数: 24 Bark (0-15500 Hz)

Bark帯域幅:
  0-500 Hz: 約100 Hz
  500-2000 Hz: 約200 Hz
  > 2000 Hz: 約 f/4 Hz
```

| Bark | 中心周波数 | 帯域幅 |
|------|----------|-------|
| 1 | 50 Hz | 80 Hz |
| 5 | 350 Hz | 100 Hz |
| 10 | 1000 Hz | 160 Hz |
| 15 | 2700 Hz | 450 Hz |
| 20 | 6400 Hz | 1100 Hz |
| 24 | 13500 Hz | 2500 Hz |

##### C API: スペクトル分析

```c
typedef struct {
    float centroid_hz;       // スペクトル重心 (Hz)
    float spread_hz;         // スペクトル拡がり (Hz)
    float flux;              // スペクトルフラックス
    float hnr_db;            // Harmonic-to-Noise Ratio (dB)
    float flatness;          // スペクトル平坦度 (0-1)
    float rolloff_hz;        // ロールオフ周波数 (Hz)
    float bark_spectrum[24]; // Bark帯域スペクトル
} ae_spectral_features_t;

ae_result_t ae_analyze_spectrum(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_spectral_features_t* out
);
```

---

#### 3.9.5 ラウドネス補正 (Loudness Correction)

##### Phon/Sone 変換

```
等ラウドネスレベル (Phon):
  1kHzの純音と同じ主観的ラウドネスを持つ音圧レベル

ラウドネス (Sone):
  40 phon = 1 sone
  
変換式 (Stevens):
  L > 40 phon: S = 2^((L - 40) / 10)
  L < 40 phon: S = (L / 40)^2.642

逆変換:
  S > 1: L = 40 + 10 × log₂(S)
  S < 1: L = 40 × S^0.378
```

| Phon | Sone | 知覚 |
|------|------|------|
| 20 | 0.25 | 非常に静か |
| 40 | 1.0 | 静か (基準) |
| 60 | 4.0 | 中程度 |
| 80 | 16.0 | 大きい |
| 100 | 64.0 | 非常に大きい |

##### 等ラウドネス曲線 (ISO 226:2003)

```
周波数ごとの等ラウドネスレベル:

40 phon等ラウドネス曲線の近似:
  L(f) = 40 + (f < 1000 ? 10 × log₁₀(1000/f) : 0)
         + (f > 4000 ? -3 × log₁₀(f/4000) : 0)

実装:
  ラウドネス補正フィルタを適用して知覚的に均一な音量調整
```

##### A/C/Z ウェイティング

```
A-weighting: 低音量での人間の聴覚感度を模倣
  - 低周波を大幅カット
  - 1-6 kHz を維持
  - 騒音測定の標準

C-weighting: 高音量での聴覚感度
  - 低周波カットが緩やか
  - ピークレベル測定に使用

Z-weighting: フラット (ウェイティングなし)
  - 物理的音圧レベル

近似フィルタ係数 (A-weighting, 48kHz):
  b = [0.2343, -0.4686, -0.2343, 0.9372, -0.2343, -0.4686, 0.2343]
  a = [1.0000, -4.0195, 6.1894, -4.4532, 1.4208, -0.1418, 0.0043]
```

##### C API: ラウドネス

```c
typedef enum {
    AE_WEIGHTING_NONE,       // Z-weighting (flat)
    AE_WEIGHTING_A,          // A-weighting
    AE_WEIGHTING_C,          // C-weighting
    AE_WEIGHTING_ITU_R_468   // ITU-R 468
} ae_weighting_t;

typedef struct {
    float loudness_sone;     // ラウドネス (sone)
    float loudness_phon;     // 等ラウドネスレベル (phon)
    float peak_db;           // ピークレベル (dBFS)
    float rms_db;            // RMSレベル (dBFS)
    float lufs;              // Integrated LUFS (EBU R128)
    float lra;               // Loudness Range (LU)
} ae_loudness_t;

// ラウドネス分析
ae_result_t ae_analyze_loudness(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_weighting_t weighting,
    ae_loudness_t* out
);

// Phon/Sone 変換
float ae_phon_to_sone(float phon);
float ae_sone_to_phon(float sone);

// ラウドネス正規化
ae_result_t ae_normalize_loudness(
    ae_engine_t* engine,
    float target_lufs        // 目標LUFS (例: -14.0)
);
```

---

### 3.10 統合パラメータ設計 (Unified Parameter Design)

ユーザーが直接操作するパラメータを最小限に抑えつつ、内部で複雑な処理を行う設計です。

#### 3.9.1 設計思想

```
ユーザー操作: 10個のシンプルなパラメータ
      ↓
内部展開: 35+個の詳細パラメータに自動変換
      ↓
処理: 各モジュールが詳細パラメータを使用
```

#### 3.8.2 Tier 1: メインパラメータ (6個)

常に表示され、基本的な音作りに使用する中核パラメータ。

| パラメータ | 範囲 | 説明 | 内部マッピング例 |
|-----------|------|------|-----------------|
| **distance** | 0.1 - 1000 m | 音源からの距離 | 伝播吸収、ドライウェット比、EQ |
| **room_size** | 0.0 - 1.0 | 空間の大きさ感 | RT60、ER密度、プリディレイ |
| **brightness** | -1.0 - 1.0 | 音の明るさ (負=暗い) | Tilt EQ、ダンピング、ハイカット |
| **width** | 0.0 - 2.0 | 空間的広がり | M/Sバランス、デコリレーション |
| **dry_wet** | 0.0 - 1.0 | 原音/エフェクト比 | 各エフェクトのウェット量 |
| **intensity** | 0.0 - 1.0 | シナリオ効果の強さ | 全パラメータのスケーリング |

#### 3.8.3 Tier 2: 拡張パラメータ (4個)

詳細な調整が必要な場合に使用するオプションパラメータ。

| パラメータ | 範囲 | 説明 | 内部マッピング例 |
|-----------|------|------|-----------------|
| **decay_time** | 0.1 - 30.0 s | 残響時間 (直接指定) | RT60 (room_sizeより優先) |
| **diffusion** | 0.0 - 1.0 | 残響の拡散度 | FDN拡散係数 |
| **lofi_amount** | 0.0 - 1.0 | ローファイ効果量 | ビット深度、ノイズ、フラッター |
| **modulation** | 0.0 - 1.0 | 変調の強さ | ピッチ揺れ、コーラス深度 |

#### 3.8.4 C API

```c
// メインパラメータ構造体
typedef struct {
    float distance;      // 0.1 - 1000 m
    float room_size;     // 0.0 - 1.0
    float brightness;    // -1.0 - 1.0
    float width;         // 0.0 - 2.0
    float dry_wet;       // 0.0 - 1.0
    float intensity;     // 0.0 - 1.0
} ae_main_params_t;

// 拡張パラメータ構造体
typedef struct {
    float decay_time;    // 0.1 - 30.0 s (0 = room_sizeから自動計算)
    float diffusion;     // 0.0 - 1.0
    float lofi_amount;   // 0.0 - 1.0
    float modulation;    // 0.0 - 1.0
} ae_extended_params_t;

// シナリオからパラメータを取得
ae_result_t ae_get_scenario_defaults(
    const char* scenario_name,
    ae_main_params_t* main_out,
    ae_extended_params_t* extended_out  // NULLでも可
);

// パラメータを適用
ae_result_t ae_set_main_params(
    ae_engine_t* engine,
    const ae_main_params_t* params
);

ae_result_t ae_set_extended_params(
    ae_engine_t* engine,
    const ae_extended_params_t* params
);

// 個別パラメータの設定 (便利関数)
ae_result_t ae_set_distance(ae_engine_t* engine, float value);
ae_result_t ae_set_room_size(ae_engine_t* engine, float value);
ae_result_t ae_set_brightness(ae_engine_t* engine, float value);
ae_result_t ae_set_width(ae_engine_t* engine, float value);
ae_result_t ae_set_dry_wet(ae_engine_t* engine, float value);
ae_result_t ae_set_intensity(ae_engine_t* engine, float value);
```

#### 3.8.5 シナリオ別デフォルト値

| シナリオ | distance | room_size | brightness | width | dry_wet | intensity |
|---------|----------|-----------|------------|-------|---------|-----------|
| deep_sea | 50.0 | 0.85 | -0.50 | 1.60 | 0.70 | 1.00 |
| cave | 20.0 | 0.70 | 0.00 | 1.50 | 0.65 | 1.00 |
| forest | 15.0 | 0.30 | -0.20 | 1.20 | 0.40 | 1.00 |
| cathedral | 30.0 | 0.90 | -0.10 | 1.60 | 0.75 | 1.00 |
| tension | 5.0 | 0.35 | 0.40 | 0.60 | 0.50 | 1.00 |
| nostalgia | 8.0 | 0.45 | -0.30 | 1.00 | 0.55 | 1.00 |
| intimate | 1.0 | 0.15 | 0.10 | 0.40 | 0.25 | 1.00 |

#### 3.8.6 内部パラメータへの変換例

`brightness = -0.5` の場合:

```c
// 内部で以下のように展開される
internal.tilt_db = -3.0f;           // Tilt EQ
internal.damping = 0.70f;           // リバーブダンピング
internal.high_cut_hz = 8000.0f;     // ハイカットフィルタ
internal.high_shelf_db = -2.0f;     // ハイシェルフ
```

`room_size = 0.85` の場合:

```c
// 内部で以下のように展開される
internal.rt60 = 8.0f;               // 残響時間
internal.pre_delay_ms = 80.0f;      // プリディレイ
internal.er_density = 0.7f;         // ER密度
internal.er_pattern = "scattered";   // ERパターン
```

---

### 3.11 聴覚モデリングモジュール (Auditory Modeling)

> **追加日**: 2026-01-21  
> **参照文書**: [260121_dev.md](260121_dev.md)

本セクションは、Auditory Modeling Toolbox (AMT) との互換性向上および聴覚科学研究での利用を目的とした追加機能を定義する。

#### 3.11.1 聴覚フィルタバンク (Auditory Filterbank)

##### Gammatone フィルタバンク

Patterson (1988)、Slaney (1993) に基づく標準的な聴覚フィルタバンク。

```
g(t) = a·t^(n-1)·cos(2πf_c·t + φ)·exp(-2πb·t)

ここで:
  n: フィルタ次数 (通常 4)
  f_c: 中心周波数 (Hz)
  b: 帯域幅パラメータ (ERB依存)
```

| パラメータ | 説明 | デフォルト値 | 範囲 |
|-----------|------|-------------|------|
| `n_channels` | チャンネル数 | 32 | 4 - 64 |
| `f_low` | 最低周波数 (Hz) | 80 | 20 - 500 |
| `f_high` | 最高周波数 (Hz) | 16000 | 4000 - 22050 |
| `filter_order` | フィルタ次数 | 4 | 1 - 8 |

```c
typedef struct ae_gammatone ae_gammatone_t;

typedef struct {
    uint32_t n_channels;
    float f_low;
    float f_high;
    uint8_t filter_order;
    uint32_t sample_rate;
} ae_gammatone_config_t;

AE_API ae_gammatone_t* ae_gammatone_create(const ae_gammatone_config_t* config);
AE_API void ae_gammatone_destroy(ae_gammatone_t* gt);
AE_API ae_result_t ae_gammatone_process(
    ae_gammatone_t* gt,
    const float* input,
    size_t n_samples,
    float** output  /* [n_channels][n_samples] */
);
```

##### DRNL フィルタバンク (Dual-Resonance Nonlinear)

Meddis et al. (2001) に基づく、より生理学的に正確な非線形蝸牛モデル。

```
構造:
  入力 ──┬── [Linear Path] ────────────────────┬── 出力
         │                                      │
         └── [Nonlinear Path] ─────────────────┘
              └─ Broken-stick compression
```

---

#### 3.11.2 内有毛細胞・適応ループ (IHC & Adaptation)

##### 内有毛細胞 (IHC) モデル

```
IHC出力 = LPF(max(0, BM_velocity))^θ

ここで:
  θ: 圧縮指数 (通常 0.3)
  LPF: 低域通過フィルタ (fc ≈ 1000-3000 Hz)
```

```c
typedef struct {
    float compression_exponent;
    float lpf_cutoff_hz;
} ae_ihc_config_t;

AE_API ae_result_t ae_ihc_envelope(
    const float* basilar_membrane_output,
    size_t n_samples,
    const ae_ihc_config_t* config,
    float* ihc_output
);
```

##### 適応ループ (Adaptation Loop)

Dau et al. (1996) の5段カスケード適応モデル。時間分解能とフォワードマスキングをモデル化。

```
各ステージ:
  dA/dt = (1/τ) × (A_min + |A - A_min|) × (input - A)

5段カスケード時定数: τ = [5, 50, 129, 253, 500] ms
```

```c
typedef struct ae_adaptloop ae_adaptloop_t;

typedef struct {
    uint8_t n_stages;
    float time_constants[5];
    float min_output;
    uint32_t sample_rate;
} ae_adaptloop_config_t;

AE_API ae_adaptloop_t* ae_adaptloop_create(const ae_adaptloop_config_t* config);
AE_API void ae_adaptloop_destroy(ae_adaptloop_t* al);
AE_API ae_result_t ae_adaptloop_process(
    ae_adaptloop_t* al,
    const float* input,
    size_t n_samples,
    float* output
);
```

---

#### 3.11.3 変調フィルタバンク (Modulation Filterbank)

Dau et al. (1997) に基づく変調スペクトル解析。音声リズムやラフネス/変動強度計算の基盤。

```
変調中心周波数: [0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256] Hz
Q値: 1 (< 10Hz), 2 (≥ 10Hz)
```

```c
typedef struct ae_modfb ae_modfb_t;

AE_API ae_modfb_t* ae_modfb_create(const ae_modfb_config_t* config);
AE_API void ae_modfb_destroy(ae_modfb_t* mfb);
AE_API ae_result_t ae_modfb_process(
    ae_modfb_t* mfb,
    const float* input,
    size_t n_samples,
    float* output  /* [n_mod_channels][n_samples] */
);
```

---

#### 3.11.4 Zwicker ラウドネス完全版 (ISO 532-1/2)

既存の簡略版を ISO 532-1 (定常音) / ISO 532-2 (時変音) 完全準拠に拡張。

```c
typedef struct {
    float specific_loudness[AE_NUM_BARK_BANDS];  /* 特定ラウドネス (sone/Bark) */
    float total_loudness_sone;
    float loudness_level_phon;
    float peak_loudness_sone;
    uint8_t peak_bark_band;
} ae_zwicker_loudness_t;

typedef enum {
    AE_LOUDNESS_METHOD_ISO_532_1,  /* 定常音 */
    AE_LOUDNESS_METHOD_ISO_532_2,  /* 時変音 */
    AE_LOUDNESS_METHOD_MOORE       /* Moore-Glasberg */
} ae_loudness_method_t;

AE_API ae_result_t ae_analyze_zwicker_loudness(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_loudness_method_t method,
    ae_zwicker_loudness_t* out
);
```

---

#### 3.11.5 音質パラメータ計算

既存の `ae_timbral_params_t` の値を実際に計算する機能を追加。

##### シャープネス計算 (DIN 45692)

```c
typedef enum {
    AE_SHARPNESS_DIN_45692,
    AE_SHARPNESS_AURES,
    AE_SHARPNESS_BISMARCK
} ae_sharpness_method_t;

AE_API ae_result_t ae_compute_sharpness(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_sharpness_method_t method,
    float* sharpness_acum
);
```

##### ラフネス計算 (Daniel & Weber)

```c
AE_API ae_result_t ae_compute_roughness(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    float* roughness_asper
);
```

##### 変動強度計算 (Fastl & Zwicker)

```c
AE_API ae_result_t ae_compute_fluctuation_strength(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    float* fluctuation_vacil
);
```

---

#### 3.11.6 統合聴覚処理パイプライン

複数の聴覚処理ステージをまとめて実行するためのAPI。

```c
typedef struct {
    float** gammatone_output;     /* [n_audio_ch][n_samples] */
    float** ihc_output;
    float** adaptation_output;
    float*** modulation_output;   /* [n_audio_ch][n_mod_ch][n_samples] */
    uint32_t n_audio_channels;
    uint32_t n_modulation_channels;
    size_t n_samples;
} ae_auditory_repr_t;

typedef struct {
    ae_gammatone_config_t gammatone;
    ae_ihc_config_t ihc;
    ae_adaptloop_config_t adaptation;
    ae_modfb_config_t modulation;
    bool compute_gammatone;
    bool compute_ihc;
    bool compute_adaptation;
    bool compute_modulation;
} ae_auditory_pipeline_config_t;

AE_API ae_result_t ae_compute_auditory_representation(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    const ae_auditory_pipeline_config_t* config,
    ae_auditory_repr_t* out
);

AE_API void ae_free_auditory_representation(ae_auditory_repr_t* repr);
```

---

#### 3.11.7 実装優先順位

| Phase | 機能 | 工数見積 |
|-------|-----|---------|
| Phase 1 (v0.2.0) | Gammatone, IHC, 適応ループ, Zwicker完全版 | 8日 |
| Phase 2 (v0.3.0) | 変調フィルタバンク, ラフネス, シャープネス, 変動強度 | 6日 |
| Phase 3 (v0.4.0) | DRNL, BMLD, SII | 6日 |

---

## 4. 非機能要件

### 4.1 パフォーマンス

| 項目 | 要件 |
|-----|------|
| **レイテンシ** | ≤50ms (リアルタイム処理) |
| **バッファサイズ** | 64 - 4096 samples 対応 |
| **サンプルレート** | 44.1kHz, 48kHz, 96kHz 対応 |
| **SIMD** | SSE2 (必須), AVX2 (推奨), ARM NEON (将来) |
| **バッチ処理** | 100並列ファイル処理対応 |

### 4.2 プラットフォーム

| プラットフォーム | 優先度 | 形式 |
|----------------|-------|------|
| Windows x64 | **Phase 1** | DLL |
| macOS (Intel/ARM) | Phase 1 (source) | dylib |
| Linux x64 | Phase 1 (source) | so |
| WebAssembly | Phase 3 | WASM |

### 4.3 言語バインディング

| 言語 | 優先度 | 用途 |
|-----|-------|------|
| C | 必須 | ネイティブAPI |
| Python (ctypes) | **Phase 1** | GUI, プロトタイピング |
| C# (P/Invoke) | Phase 2 | Unity連携 |
| JavaScript | Phase 3 | WebAudio, TouchDesigner |

### 4.4 API詳細仕様

#### 4.4.1 オーディオバッファ仕様

```c
// オーディオバッファ構造体
typedef struct {
    float* samples;          // サンプルデータ (16バイトアライン推奨)
    size_t frame_count;      // フレーム数 (64 - 4096)
    uint8_t channels;        // チャンネル数 (1 = mono, 2 = stereo)
    bool interleaved;        // true: LRLRLR, false: LLL...RRR (planar)
} ae_audio_buffer_t;

// 処理関数
ae_result_t ae_process(
    ae_engine_t* engine,
    const ae_audio_buffer_t* input,   // 入力 (NULLでサイレンス)
    ae_audio_buffer_t* output         // 出力 (input と同一可 = inplace)
);
```

| 項目 | 仕様 |
|-----|------|
| サンプルレート | 48000 Hz (固定) |
| ビット深度 | 32-bit float (-1.0 〜 +1.0) |
| チャンネル | モノラル入力 → ステレオ出力 |
| バッファアライメント | 16バイト境界推奨 (SIMD効率) |
| inplace処理 | 許可 (input == output) |

#### 4.4.2 エラー処理

```c
// 結果コード
typedef enum {
    AE_OK = 0,                       // 成功
    AE_ERROR_INVALID_PARAM = -1,     // 無効なパラメータ
    AE_ERROR_OUT_OF_MEMORY = -2,     // メモリ不足
    AE_ERROR_FILE_NOT_FOUND = -3,    // ファイルが見つからない
    AE_ERROR_INVALID_PRESET = -4,    // 無効なプリセット
    AE_ERROR_HRTF_LOAD_FAILED = -5,  // HRTF読み込み失敗
    AE_ERROR_JSON_PARSE = -6,        // JSONパースエラー
    AE_ERROR_BUFFER_TOO_SMALL = -7,  // バッファサイズ不足
    AE_ERROR_NOT_INITIALIZED = -8,   // 未初期化
} ae_result_t;

// エラー詳細取得
const char* ae_get_error_string(ae_result_t result);
const char* ae_get_last_error_detail(ae_engine_t* engine);
```

#### 4.4.3 スレッドセーフティ

| 操作 | スレッドセーフ | 備考 |
|-----|--------------|------|
| `ae_process()` | ✅ リアルタイムセーフ | メモリ確保なし、ロックなし |
| `ae_set_*()` パラメータ設定 | ✅ ロックフリー | アトミック更新、次回process()で反映 |
| `ae_load_preset()` | ⚠️ 非同期推奨 | JSONパース中はブロック可能性あり |
| `ae_create_engine()` | ❌ | 単一スレッドから呼び出し |
| `ae_destroy_engine()` | ❌ | 他スレッドがprocess中は禁止 |

```c
// リアルタイムスレッド
void audio_callback(float* in, float* out, size_t frames) {
    ae_process(engine, &input_buf, &output_buf);  // 常に安全
}

// UIスレッド
void on_slider_change(float value) {
    ae_set_room_size(engine, value);  // ロックフリーで安全
}

// バックグラウンドスレッド (非同期読み込み)
void load_preset_async(const char* name) {
    ae_result_t result = ae_load_preset(engine, name);
    // 読み込み完了後、次のae_process()から反映
}
```

#### 4.4.4 パラメータ優先順位

パラメータは以下の順序で適用される（後から適用されたものが優先）:

```
1. プリセット (ae_load_preset)
   ↓ ベースパラメータを設定
2. メインパラメータ (ae_set_main_params / ae_set_*)
   ↓ プリセット値を上書き
3. 拡張パラメータ (ae_set_extended_params)
   ↓ 詳細調整
4. 環境モデル (ae_apply_cave_model 等)
   ↓ 物理モデル固有のパラメータを追加/上書き
```

```c
// 例: deep_seaベースに距離だけ変更
ae_load_preset(engine, "deep_sea");      // 全パラメータ設定
ae_set_distance(engine, 100.0f);          // distance だけ上書き
// → 他のパラメータはdeep_seaのまま、distanceだけ100m
```

#### 4.4.5 リソース管理

```c
// エンジン設定
typedef struct {
    uint32_t sample_rate;            // 48000 (固定)
    uint32_t max_buffer_size;        // 最大バッファ (デフォルト: 4096)
    const char* data_path;           // データディレクトリ (NULL = 実行ファイルと同階層)
    
    // HRTF設定
    const char* hrtf_path;           // HRTFファイルパス (NULL = 内蔵HRTFを使用)
    bool preload_hrtf;               // true = 起動時読み込み (推奨)
    
    // プリセット設定
    bool preload_all_presets;        // true = 全プリセットを起動時読み込み
    
    // メモリ設定
    size_t max_reverb_time_sec;      // 最大リバーブ時間 (デフォルト: 10秒)
                                     // → メモリ使用量に影響
} ae_config_t;

// デフォルト設定の取得
ae_config_t ae_get_default_config(void);
```

**メモリ使用量の目安**:

| 設定 | メモリ使用量 |
|-----|------------|
| 基本エンジン | 約 5 MB |
| HRTF (MIT KEMAR) | 約 2 MB |
| FDN Reverb (10秒, 8ch) | 約 15 MB |
| FDN Reverb (30秒, 8ch) | 約 46 MB |
| **合計 (標準構成)** | **約 22 MB** |

---

#### 4.4.6 ????????????? (???????)

??: ??????????????????????????????????????

| ?? | ?? |
|-----|------|
| ??????(??) | WAV, MP3, M4A (AAC/ALAC), ANO |
| ?????? | MP3/M4A/ANO?????????? (ffmpeg??)???????`AE_FFMPEG_PATH` ?????? |
| ??????(??) | FLAC, AIFF, OGG/Vorbis, Opus, CAF, WMA |
| ?? | ???????????/?????????? |
| ??????? | 8 kHz - 192 kHz ????????? 48 kHz ?????? |
| ????? | 16/24/32-bit int, 32-bit float ????? |
| ????? | 1 - 8ch ????????? mono/stereo ???????? |
| ??? | ???? float32 [-1.0, +1.0] ??? |
| ??? | ???/??? AE_ERROR_INVALID_PARAM ??????? ae_get_last_error_detail() ??? |

**??**: ?????????????WAV????????

**??**: ?????(?????)??????????????? `ae_audio_buffer_t` ???????

---

### 4.5 SIMD最適化仕様

#### 4.5.1 対象命令セット

| 命令セット | 優先度 | ベクトル幅 | 対象プラットフォーム |
|-----------|-------|----------|-------------------|
| **SSE2** | 必須 | 128-bit (4 float) | x86_64 すべて |
| **AVX2** | 推奨 | 256-bit (8 float) | 2013年以降のIntel/AMD |
| **AVX-512** | 将来 | 512-bit (16 float) | サーバー向け |
| **ARM NEON** | Phase 2 | 128-bit (4 float) | Apple Silicon, RPi |

#### 4.5.2 SIMDで実装する処理

| 処理 | SIMD化 | 詳細 |
|-----|-------|------|
| **バッファコピー/加算** | ✅ 必須 | 基本操作、全処理で使用 |
| **Biquadフィルタ** | ✅ 必須 | EQ、ハイカット、ローカット |
| **FDNディレイ読み書き** | ✅ 必須 | 8チャンネル並列処理 |
| **Hadamard行列演算** | ✅ 必須 | FDNフィードバック |
| **FFT/IFFT** | ✅ 必須 | HRTF畳み込み (pffftに依存) |
| **複素数乗算** | ✅ 必須 | 周波数領域畳み込み |
| **ゲイン適用** | ✅ 必須 | 全モジュールで使用 |
| **線形補間** | ✅ 推奨 | ディレイライン、HRTF補間 |
| **パラメータスムージング** | ⚪ オプション | 1次IIRフィルタ |

#### 4.5.3 SIMD最適化の実装方針

```c
// コンパイル時のSIMD検出
#if defined(__AVX2__)
    #define AE_SIMD_WIDTH 8
    #include "simd_avx.h"
#elif defined(__SSE2__)
    #define AE_SIMD_WIDTH 4
    #include "simd_sse.h"
#else
    #define AE_SIMD_WIDTH 1
    #include "simd_scalar.h"
#endif

// 共通インターフェース
void ae_simd_add(float* dst, const float* a, const float* b, size_t n);
void ae_simd_mul(float* dst, const float* a, const float* b, size_t n);
void ae_simd_scale(float* dst, const float* src, float scale, size_t n);
void ae_simd_mac(float* dst, const float* a, const float* b, size_t n); // dst += a * b
```

#### 4.5.4 具体的なSIMD実装例

##### Biquadフィルタ (SSE2)

```c
// 4サンプル並列処理
void biquad_process_sse(
    float* output,
    const float* input,
    size_t frames,
    const float* coeffs,  // b0, b1, b2, a1, a2
    float* state          // z1, z2
) {
    __m128 b0 = _mm_set1_ps(coeffs[0]);
    __m128 b1 = _mm_set1_ps(coeffs[1]);
    __m128 b2 = _mm_set1_ps(coeffs[2]);
    __m128 a1 = _mm_set1_ps(coeffs[3]);
    __m128 a2 = _mm_set1_ps(coeffs[4]);
    
    // 4サンプルずつ処理
    for (size_t i = 0; i < frames; i += 4) {
        __m128 x = _mm_load_ps(&input[i]);
        // Direct Form II Transposed
        __m128 y = _mm_add_ps(_mm_mul_ps(b0, x), z1);
        z1 = _mm_add_ps(_mm_mul_ps(b1, x), 
             _mm_sub_ps(z2, _mm_mul_ps(a1, y)));
        z2 = _mm_sub_ps(_mm_mul_ps(b2, x), _mm_mul_ps(a2, y));
        _mm_store_ps(&output[i], y);
    }
}
```

##### FDN Hadamard行列 (AVX2)

```c
// 8x8 Hadamard変換 (AVX2で8チャンネル同時処理)
void fdn_hadamard_8x8_avx(float* channels) {
    __m256 a = _mm256_load_ps(channels);
    
    // Hadamard変換 (バタフライ演算)
    // Stage 1: 隣接ペア
    __m256 b = _mm256_permute_ps(a, 0xB1);  // swap pairs
    __m256 c = _mm256_addsub_ps(a, b);
    
    // Stage 2: 4要素グループ
    __m256 d = _mm256_permute2f128_ps(c, c, 0x01);
    __m256 e = _mm256_add_ps(c, d);
    __m256 f = _mm256_sub_ps(c, d);
    
    // 正規化 (1/sqrt(8))
    __m256 norm = _mm256_set1_ps(0.353553f);
    __m256 result = _mm256_mul_ps(e, norm);
    
    _mm256_store_ps(channels, result);
}
```

##### ゲイン適用とミックス (AVX2)

```c
// dst = src * gain (AVX2: 8サンプル同時)
void apply_gain_avx(
    float* dst, 
    const float* src, 
    float gain, 
    size_t frames
) {
    __m256 g = _mm256_set1_ps(gain);
    
    size_t i = 0;
    // メインループ (8サンプルずつ)
    for (; i + 8 <= frames; i += 8) {
        __m256 x = _mm256_load_ps(&src[i]);
        __m256 y = _mm256_mul_ps(x, g);
        _mm256_store_ps(&dst[i], y);
    }
    // 端数処理
    for (; i < frames; i++) {
        dst[i] = src[i] * gain;
    }
}

// dst += src * gain (MAC: Multiply-Accumulate)
void mix_with_gain_avx(
    float* dst, 
    const float* src, 
    float gain, 
    size_t frames
) {
    __m256 g = _mm256_set1_ps(gain);
    
    for (size_t i = 0; i + 8 <= frames; i += 8) {
        __m256 d = _mm256_load_ps(&dst[i]);
        __m256 s = _mm256_load_ps(&src[i]);
        __m256 r = _mm256_fmadd_ps(s, g, d);  // FMA: d + s * g
        _mm256_store_ps(&dst[i], r);
    }
}
```

#### 4.5.5 パフォーマンス目標

| 処理 | SSE2 | AVX2 | 目標 (48kHz, 256frames) |
|-----|------|------|------------------------|
| Biquad (1バンド) | 2.5x | 5x | < 0.1 ms |
| FDN 8ch | 3x | 6x | < 0.5 ms |
| HRTF畳み込み | 4x | 7x | < 1.0 ms |
| **全処理チェーン** | - | - | **< 5 ms (10% CPU @ 48kHz)** |

---

## 5. アーキテクチャ

### 5.1 ディレクトリ構成

```
acoustic_engine/
│
├── CMakeLists.txt              # ルートビルド設定
├── README.md                   # プロジェクト概要
├── LICENSE                     # ライセンス
│
├── include/                    # 公開ヘッダ (DLL利用者が参照)
│   └── acoustic_engine/
│       ├── ae_core.h           # 基本型、エンジン作成/破棄
│       ├── ae_params.h         # パラメータ構造体 (main/extended)
│       ├── ae_preset.h         # プリセットAPI
│       ├── ae_process.h        # オーディオ処理API
│       └── acoustic_engine.h   # 統合ヘッダ (全部include)
│
├── src/                        # 実装 (非公開)
│   ├── core/                   # 基盤
│   │   ├── engine.c            # エンジン管理
│   │   ├── buffer.c            # オーディオバッファ
│   │   ├── memory.c            # メモリプール
│   │   └── simd/               # SIMD実装
│   │       ├── simd_sse.c
│   │       └── simd_avx.c
│   │
│   ├── dsp/                    # DSPプリミティブ
│   │   ├── fft.c               # pffft wrapper
│   │   ├── biquad.c            # IIRフィルタ
│   │   ├── delay.c             # ディレイライン
│   │   └── convolver.c         # 畳み込み
│   │
│   ├── modules/                # エフェクトモジュール
│   │   ├── propagation.c       # 物理伝播 (Francois-Garrison等)
│   │   ├── hrtf.c              # HRTF処理
│   │   ├── reverb_fdn.c        # FDNリバーブ
│   │   ├── reverb_er.c         # Early Reflections
│   │   ├── ms_processor.c      # M/S処理
│   │   ├── dynamics.c          # コンプ/リミッター
│   │   ├── eq.c                # EQ (Tilt, Parametric)
│   │   ├── lofi.c              # ローファイ効果
│   │   └── saturation.c        # サチュレーション
│   │
│   ├── params/                 # パラメータ管理
│   │   ├── param_resolver.c    # main→internal変換
│   │   └── preset_loader.c     # JSON読み込み
│   │
│   └── io/                     # 入出力
│       └── audio_io.c          # ????????/???? (WAV/MP3/M4A/ANO ??)
│
├── data/                       # ランタイムデータ (DLLと一緒に配布)
│   ├── hrtf/                   # HRTFファイル
│   │   └── mit_kemar.sofa      # デフォルトHRTF (MIT KEMAR)
│   └── presets/                # プリセット定義 (JSON)
│       ├── deep_sea.json
│       ├── cave.json
│       ├── forest.json
│       ├── cathedral.json
│       ├── tension.json
│       ├── nostalgia.json
│       └── intimate.json
│
├── extern/                     # 外部ライブラリ (git submodule)
│   ├── pffft/                  # FFTライブラリ
│   └── libmysofa/              # SOFA読み込み
│
├── bindings/                   # 言語バインディング
│   └── python/
│       ├── acoustic_engine.py  # ctypes DLL wrapper
│       └── example.py          # 使用例
│
├── tools/                      # CLIツール
│   └── ae_process.c            # バッチ処理CLI
│
├── tests/                      # テスト
│   ├── test_reverb.c
│   ├── test_propagation.c
│   └── fixtures/               # テスト用音声
│       └── test_mono_48k.wav
│
├── docs/                       # ドキュメント
│   ├── requirements_ja.md      # 要件定義書 (日本語)
│   ├── requirements_en.md      # 要件定義書 (英語)
│   └── api_reference.md        # API詳細
│
└── build/                      # ビルド出力 (.gitignore)
    ├── acoustic_engine.dll     # Windows DLL
    ├── acoustic_engine.lib     # インポートライブラリ
    └── ...
```

### 5.2 プリセットJSON形式

```json
{
  "name": "deep_sea",
  "description": "深海のような音響環境",
  "main_params": {
    "distance": 50.0,
    "room_size": 0.85,
    "brightness": -0.50,
    "width": 1.60,
    "dry_wet": 0.70,
    "intensity": 1.00
  },
  "extended_params": {
    "decay_time": 8.0,
    "diffusion": 0.85,
    "lofi_amount": 0.0,
    "modulation": 0.20
  },
  "internal_overrides": {
    "absorption_model": "francois_garrison",
    "temperature": 5.0,
    "depth": 200.0
  }
}
```

### 5.3 CMake設定

```cmake
cmake_minimum_required(VERSION 3.16)
project(acoustic_engine VERSION 0.1.0 LANGUAGES C)

# オプション
option(AE_BUILD_TESTS "Build tests" ON)
option(AE_BUILD_TOOLS "Build CLI tools" ON)
option(AE_USE_AVX2 "Enable AVX2 optimizations" ON)
option(AE_ENABLE_EXTERNAL_DECODER "Enable external decoder for non-WAV formats" ON)

# 出力ディレクトリ
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 外部ライブラリ
add_subdirectory(extern/pffft)
add_subdirectory(extern/libmysofa)

# メインライブラリ (DLL)
file(GLOB_RECURSE AE_SOURCES src/*.c)
add_library(acoustic_engine SHARED ${AE_SOURCES})

target_include_directories(acoustic_engine PUBLIC include)
target_link_libraries(acoustic_engine PRIVATE pffft mysofa)

# Windows DLLエクスポート
if(WIN32)
    target_compile_definitions(acoustic_engine PRIVATE AE_EXPORT)
endif()

# SIMD設定
if(AE_USE_AVX2)
    if(MSVC)
        target_compile_options(acoustic_engine PRIVATE /arch:AVX2)
    else()
        target_compile_options(acoustic_engine PRIVATE -mavx2)
    endif()
endif()

# データファイルのコピー
add_custom_command(TARGET acoustic_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/bin/data
)

# テスト
if(AE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# CLIツール
if(AE_BUILD_TOOLS)
    add_executable(ae_process tools/ae_process.c)
    target_link_libraries(ae_process acoustic_engine)
endif()
```

### 5.4 Python ctypes Wrapper

```python
# bindings/python/acoustic_engine.py
import ctypes
from pathlib import Path

class AcousticEngine:
    def __init__(self, dll_path=None):
        if dll_path is None:
            dll_path = Path(__file__).parent / "acoustic_engine.dll"
        self._lib = ctypes.CDLL(str(dll_path))
        self._setup_functions()
        self._engine = self._lib.ae_create_engine(None)
    
    def _setup_functions(self):
        # ae_create_engine
        self._lib.ae_create_engine.restype = ctypes.c_void_p
        self._lib.ae_create_engine.argtypes = [ctypes.c_void_p]
        
        # ae_destroy_engine
        self._lib.ae_destroy_engine.argtypes = [ctypes.c_void_p]
        
        # ae_load_preset
        self._lib.ae_load_preset.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self._lib.ae_load_preset.restype = ctypes.c_int
        
        # ae_set_* (パラメータ設定)
        for param in ['distance', 'room_size', 'brightness', 
                      'width', 'dry_wet', 'intensity']:
            fn = getattr(self._lib, f'ae_set_{param}')
            fn.argtypes = [ctypes.c_void_p, ctypes.c_float]
            fn.restype = ctypes.c_int
    
    def load_preset(self, name: str):
        return self._lib.ae_load_preset(self._engine, name.encode())
    
    def set_distance(self, value: float):
        return self._lib.ae_set_distance(self._engine, value)
    
    def set_room_size(self, value: float):
        return self._lib.ae_set_room_size(self._engine, value)
    
    # ... 他のパラメータも同様
    
    def __del__(self):
        if hasattr(self, '_engine') and self._engine:
            self._lib.ae_destroy_engine(self._engine)
```

### 5.5 C API設計方針

```c
// 1. エンジン作成・破棄
ae_engine_t* ae_create_engine(const ae_config_t* config);
void         ae_destroy_engine(ae_engine_t* engine);

// 2. モジュール取得
ae_hrtf_t*      ae_get_hrtf(ae_engine_t* engine);
ae_reverb_t*    ae_get_reverb(ae_engine_t* engine);
ae_dynamics_t*  ae_get_dynamics(ae_engine_t* engine);

// 3. 処理
ae_result_t ae_process(ae_engine_t* engine, 
                       float* input, 
                       float* output, 
                       size_t frames);

// 4. セマンティック表現
ae_result_t ae_apply_expression(ae_engine_t* engine,
                                const char* expression);

// 5. 生体信号更新
ae_result_t ae_update_biosignal(ae_engine_t* engine,
                                ae_biosignal_type_t type,
                                float value);
```

---

## 6. 外部依存

| ライブラリ | 用途 | ライセンス |
|-----------|------|-----------|
| **pffft** | FFT/IFFT | BSD-like |
| **SOFA (libmysofa)** | HRTF読み込み | BSD-3-Clause |

---

## 7. 開発計画

### Phase 1: Core + Deep Sea (8週間)

- [ ] DSPコア (SIMD, FFT, フィルタ, ディレイ)
- [ ] Francois-Garrison吸収
- [ ] HRTF (SOFA読み込み, 畳み込み, 補間)
- [ ] FDNリバーブ
- [ ] M/S処理
- [ ] Python バインディング

### Phase 2: Semantic + Biosignal (4週間)

- [ ] Expression パーサー
- [ ] プリセットシステム
- [ ] 心拍マッピング
- [ ] サイドチェインコンプ

### Phase 3: Mastering + Polish (4週間)

- [ ] Tilt EQ, デエッサー
- [ ] ルックアヘッドリミッター
- [ ] C# バインディング
- [ ] ドキュメント整備

### Phase 4: Expansion (将来)

- [ ] WebAssembly対応
- [ ] 畳み込みリバーブ (IR)
- [ ] 大気伝播 (ISO 9613)
- [ ] 個人化HRTF

---

## 8. 用語集

| 用語 | 説明 |
|-----|------|
| **HRTF** | Head-Related Transfer Function - 頭部伝達関数 |
| **HRIR** | Head-Related Impulse Response - HRTFの時間領域表現 |
| **IACC** | Interaural Cross-Correlation - 両耳相互相関、空間的広がりの指標 |
| **FDN** | Feedback Delay Network - 残響合成アルゴリズム |
| **M/S** | Mid/Side - ステレオ信号の処理方式 |
| **SOFA** | Spatially Oriented Format for Acoustics - HRTF標準フォーマット |
| **RT60** | 残響時間 - 60dB減衰にかかる時間 |

---

## 付録A: Francois-Garrison式の詳細

海水中の音波吸収係数は以下で計算される:

```
α(f) = (A₁·P₁·f₁·f²)/(f₁² + f²) + (A₂·P₂·f₂·f²)/(f₂² + f²) + A₃·P₃·f²

ここで:
  f   : 周波数 [kHz]
  A₁  : ホウ酸の寄与係数
  A₂  : 硫酸マグネシウムの寄与係数
  A₃  : 純水の寄与係数
  f₁  : ホウ酸の緩和周波数 [kHz]
  f₂  : 硫酸マグネシウムの緩和周波数 [kHz]
  P₁,P₂,P₃ : 圧力補正係数

実装では、温度T[°C]、塩分S[ppt]、深度D[m]から各係数を導出する。
```

---

## 付録B: 参考文献

### 海洋音響学
1. Francois, R.E. & Garrison, G.R. (1982). "Sound absorption based on ocean measurements: Part I". *J. Acoust. Soc. Am.*, 72(3), 896-907.
2. Francois, R.E. & Garrison, G.R. (1982). "Sound absorption based on ocean measurements: Part II". *J. Acoust. Soc. Am.*, 72(6), 1879-1890.

### 大気音響学
3. ISO 9613-1:1993. "Acoustics — Attenuation of sound during propagation outdoors — Part 1: Calculation of the absorption of sound by the atmosphere".

### 室内音響学
4. Sabine, W.C. (1922). "Collected Papers on Acoustics". Harvard University Press.
5. Eyring, C.F. (1930). "Reverberation Time in 'Dead' Rooms". *J. Acoust. Soc. Am.*, 1(2A), 217-241.
6. Jot, J.M. (1992). "An analysis/synthesis approach to real-time artificial reverberation". *ICASSP*.
7. Beranek, L.L. (2004). "Concert Halls and Opera Houses: Music, Acoustics, and Architecture". Springer.

### 心理音響学・空間知覚
8. Blauert, J. (1997). "Spatial Hearing: The Psychophysics of Human Sound Localization". MIT Press.
9. Zwicker, E. & Fastl, H. (1999). "Psychoacoustics: Facts and Models". Springer.
10. Zahorik, P. (2002). "Assessing auditory distance perception using virtual acoustics". *J. Acoust. Soc. Am.*, 111(4), 1832-1846.
11. Bronkhorst, A.W. & Houtgast, T. (1999). "Auditory distance perception in rooms". *Nature*, 397, 517-520.
12. Griesinger, D. (1997). "The psychoacoustics of apparent source width, spaciousness and envelopment in performance spaces". *Acta Acustica*, 83, 721-731.

### HRTF・両耳聴
13. Algazi, V.R. et al. (2001). "The CIPIC HRTF Database". *IEEE ASSP Workshop*.
14. Møller, H. et al. (1995). "Head-Related Transfer Functions of Human Subjects". *J. Audio Eng. Soc.*, 43(5), 300-321.

### 音楽心理学・感情
15. Juslin, P.N. & Laukka, P. (2004). "Expression, Perception, and Induction of Musical Emotions". *Journal of New Music Research*, 33(3), 217-238.
16. Västfjäll, D. (2012). "Emotional reactions to sounds without meaning". *Psychology of Music*, 40(4), 450-467.
17. Huron, D. (2006). "Sweet Anticipation: Music and the Psychology of Expectation". MIT Press.

### 測定・規格
18. ISO 226:2003. "Acoustics — Normal equal-loudness-level contours".
19. Farina, A. (2000). "Simultaneous measurement of impulse response and distortion with a swept-sine technique". *AES Convention*.
20. Cox, T.J. & D'Antonio, P. (2009). "Acoustic Absorbers and Diffusers: Theory, Design and Application". Taylor & Francis.

---

## 付録C: 科学的限界と近似

本ライブラリは芸術的・実用的目的で設計されており、以下の点で厳密な物理シミュレーションとは異なります。

### C.1 物理モデルの近似

| モデル | 近似・簡略化 | 影響 |
|-------|------------|------|
| **Francois-Garrison** | 均質媒質を仮定 | 温度躍層などの効果は無視 |
| **ISO 9613** | ルックアップテーブル化 | 連続値からの離散化誤差 |
| **洞窟モード共鳴** | 一次元近似 | 実際の不規則形状は再現不可 |
| **RT60計算** | Eyring式 | 非拡散音場では不正確 |

### C.2 知覚モデルの限界

| パラメータ | 限界 | 個人差の影響 |
|-----------|------|------------|
| **brightness** | 周波数重心との相関は近似 | 年齢による高周波感度低下 |
| **width (IACC)** | 測定窓・条件依存 | 頭部サイズ、耳介形状 |
| **distance** | 知覚距離は^0.54乗則近似 | 環境適応による変動 |
| **intimacy** | 主観的概念の定量化 | 文化・経験差 |

### C.3 芸術的選択

以下の処理は物理的正確性より知覚的効果を優先しています:

1. **深海プリセット**: 開放水域では本来RT60は定義されないが、知覚的な「包囲感」のために残響を付加
2. **tension/warmth**: 心理学研究に基づくが、音楽ジャンルによる印象差は考慮外
3. **ローファイ効果**: 歴史的機器の忠実再現ではなく、「ローファイらしさ」の知覚的再現

### C.4 推奨される使用方法

| 用途 | 適合性 | 注意点 |
|-----|-------|--------|
| メディアアート | ⭐⭐⭐⭐⭐ | 主目的。知覚効果重視 |
| VR/ゲーム音響 | ⭐⭐⭐⭐☆ | 十分だが厳密なシミュレーションではない |
| 建築音響予測 | ⭐⭐☆☆☆ | 専用ソフトウェアを推奨 |
| 科学的音響研究 | ⭐⭐☆☆☆ | 参考程度。原著論文を確認のこと |

---

## 付録D: 数学的定義と数値安定性

### D.1 積分・総和の厳密な定義

#### D.1.1 IACC (Interaural Cross-Correlation)

```
連続時間定義 (ISO 3382-1:2009):

IACC = max |IACF(τ)|  for τ ∈ [-1ms, +1ms]

IACF(τ) = ∫_{t₁}^{t₂} pL(t) × pR(t+τ) dt
          ────────────────────────────────────────
          √(∫_{t₁}^{t₂} pL²(t)dt × ∫_{t₁}^{t₂} pR²(t)dt)

時間窓の定義:
  IACC_E (Early): t₁=0, t₂=80ms   → ASW (音源幅) に対応
  IACC_L (Late):  t₁=80ms, t₂=∞   → LEV (包囲感) に対応
  IACC_A (All):   t₁=0, t₂=∞      → 全体

離散実装:
  n₁ = floor(t₁ × fs), n₂ = floor(t₂ × fs)
  τ_samples = round(τ × fs), |τ| ≤ ceil(0.001 × fs)
  
  IACF[τ] = Σ_{n=n₁}^{n₂} hL[n] × hR[n+τ]
            ─────────────────────────────────
            √(Σ hL²[n] × Σ hR²[n])
```

#### D.1.2 Clarity (C50/C80)

```
連続時間定義:
  C_t = 10 × log₁₀(∫₀^t h²(τ)dτ / ∫_t^∞ h²(τ)dτ)  [dB]

離散実装 (fs = 48000 Hz):
  n_t = floor(t × fs)
  
  E_early = Σ_{n=0}^{n_t-1} h²[n]
  E_late  = Σ_{n=n_t}^{N-1} h²[n]
  
  C_t = 10 × log₁₀(E_early / E_late)

境界サンプルの扱い:
  n_50 = floor(0.050 × 48000) = 2400  (50ms)
  n_80 = floor(0.080 × 48000) = 3840  (80ms)
  
  注: 境界サンプル h[n_t] は E_late に含める (ISO準拠)
```

#### D.1.3 スペクトル重心 (Spectral Centroid)

```
定義 (離散FFT):
  SC = Σ_{k=1}^{N/2} f[k] × |X[k]|² 
       ─────────────────────────────
       Σ_{k=1}^{N/2} |X[k]|²

ここで:
  f[k] = k × fs / N  [Hz]
  X[k] = FFT(x)[k]   (複素数)
  N = FFTサイズ (例: 4096)
  fs = サンプリング周波数 (例: 48000)

注意点:
  - k=0 (DC成分) は除外する (周波数0で無意味な重み)
  - k=N/2 (ナイキスト) は含める
  - 窓関数適用後のスペクトルを使用

正規化オプション:
  SC_norm = SC / (fs/2)  → [0, 1] に正規化
```

---

### D.2 参照式の明確化

#### D.2.1 Bark変換 (Traunmüller, 1990)

```
本ライブラリで使用する式:

z(f) = 26.81 × f / (1960 + f) - 0.53

  適用範囲: f ∈ [20, 15500] Hz
  出力範囲: z ∈ [0, 24] Bark

境界補正 (Traunmüller):
  z < 2:  z' = z + 0.15 × (2 - z)
  z > 20.1: z' = z + 0.22 × (z - 20.1)

逆変換:
  f(z) = 1960 × (z + 0.53) / (26.28 - z)

代替式 (Wang et al., 1992):
  z(f) = 6 × arcsinh(f / 600)
  f(z) = 600 × sinh(z / 6)

参照: Traunmüller, H. (1990). "Analytical expressions for the 
      tonotopic sensory scale." J. Acoust. Soc. Am. 88(1).
```

#### D.2.2 ISO 9613-1 モル濃度比

```
相対湿度から水蒸気モル濃度比への変換:

h = hr × (psat / pa) × (ps / pa)

ここで:
  h: 水蒸気モル濃度比
  hr: 相対湿度 (0 ≤ hr ≤ 1)
  pa: 大気圧 [Pa]
  ps: 標準大気圧 (101325 Pa)
  psat: 飽和水蒸気圧 [Pa]

飽和水蒸気圧 (ISO 9613-1, Annex B):
  psat = ps × 10^(-6.8346 × (T₀₁/T)^1.261 + 4.6151)

ここで:
  T: 絶対温度 [K]
  T₀₁ = 273.16 K (水の三重点)

実装例 (C):
  float calc_molar_concentration(float temp_c, float rh_percent) {
      float T = temp_c + 273.15f;
      float T01 = 273.16f;
      float psat = 101325.0f * powf(10.0f, 
                   -6.8346f * powf(T01/T, 1.261f) + 4.6151f);
      return (rh_percent / 100.0f) * (psat / 101325.0f);
  }
```

#### D.2.3 Roughness (Daniel & Weber, 1997)

```
本ライブラリで使用するモデル:

R = cal × Σᵢ gᵢ × ΔLᵢ² × fmod,ᵢ / (fmod,ᵢ/f₀ + f₀/fmod,ᵢ)

ここで:
  R: roughness [asper]
  cal = 0.25 (較正定数)
  i: 臨界帯域インデックス (1-24)
  gᵢ: 帯域iの重み係数 (周波数依存)
  ΔLᵢ: 帯域iのラウドネス変調深度 [sone]
  fmod,ᵢ: 帯域iの変調周波数 [Hz]
  f₀ = 70 Hz (最大感度周波数)

変調周波数の検出:
  - 各Bark帯域でエンベロープ抽出
  - エンベロープのスペクトル分析
  - 15-300 Hz の主要成分を fmod とする

参照: Daniel, P. & Weber, R. (1997). "Psychoacoustical roughness:
      Implementation of an optimized model." Acta Acustica, 83.
```

---

### D.3 数値安定性

#### D.3.1 対数計算

```c
// 問題: log(0) = -∞, log(負) = NaN

// 対策1: 下限クランプ
#define AE_LOG_EPSILON 1e-10f
float safe_log10(float x) {
    return log10f(fmaxf(x, AE_LOG_EPSILON));
}

// 対策2: dB計算
float to_db(float linear) {
    if (linear <= 0.0f) return -120.0f;  // 下限
    float db = 20.0f * log10f(linear);
    return fmaxf(db, -120.0f);  // 下限クランプ
}

// 対策3: 比の対数
float ratio_to_db(float num, float denom) {
    if (denom <= AE_LOG_EPSILON) return 0.0f;  // 定義不能
    return 10.0f * log10f(fmaxf(num / denom, AE_LOG_EPSILON));
}
```

#### D.3.2 除算

```c
// 問題: 0除算

// IACC計算での対策
float calculate_iacc(float* hL, float* hR, size_t n) {
    float eL = 0.0f, eR = 0.0f;
    for (size_t i = 0; i < n; i++) {
        eL += hL[i] * hL[i];
        eR += hR[i] * hR[i];
    }
    
    float denom = sqrtf(eL * eR);
    if (denom < AE_LOG_EPSILON) {
        return 1.0f;  // 無音の場合、完全相関と定義
    }
    
    // 相互相関計算...
    return max_correlation / denom;
}

// スペクトル重心での対策
float spectral_centroid(float* magnitude, size_t n_bins, float bin_freq) {
    float sum_fm = 0.0f, sum_m = 0.0f;
    
    for (size_t k = 1; k < n_bins; k++) {
        float f = k * bin_freq;
        float m = magnitude[k] * magnitude[k];
        sum_fm += f * m;
        sum_m += m;
    }
    
    if (sum_m < AE_LOG_EPSILON) {
        return 0.0f;  // 無音の場合
    }
    return sum_fm / sum_m;
}
```

#### D.3.3 累積誤差

```c
// 問題: 長時間処理での浮動小数点累積誤差

// 対策1: Kahan summation
typedef struct {
    float sum;
    float c;  // 補正項
} KahanSum;

void kahan_add(KahanSum* ks, float value) {
    float y = value - ks->c;
    float t = ks->sum + y;
    ks->c = (t - ks->sum) - y;
    ks->sum = t;
}

// 対策2: double精度での累積
double compute_energy(const float* samples, size_t n) {
    double energy = 0.0;
    for (size_t i = 0; i < n; i++) {
        energy += (double)samples[i] * (double)samples[i];
    }
    return energy;
}

// 対策3: ブロック単位での正規化
// 64サンプルごとにDC除去と正規化を適用
```

#### D.3.4 浮動小数点特殊値

```c
// NaN/Inf の検出と処理
#include <math.h>

float sanitize_output(float x) {
    if (isnan(x)) return 0.0f;
    if (isinf(x)) return (x > 0) ? 1.0f : -1.0f;
    return fmaxf(-1.0f, fminf(1.0f, x));  // クランプ
}

// 入力検証
ae_result_t validate_params(const ae_main_params_t* p) {
    if (isnan(p->distance) || p->distance <= 0.0f)
        return AE_ERROR_INVALID_PARAM;
    if (isnan(p->room_size) || p->room_size < 0.0f || p->room_size > 1.0f)
        return AE_ERROR_INVALID_PARAM;
    // ...
    return AE_OK;
}
```

---

### D.4 数値精度の仕様

| 項目 | 型 | 範囲 | 精度 |
|-----|---|------|------|
| オーディオサンプル | float | [-1.0, 1.0] | 約7桁 |
| 周波数 | float | [0, 24000] Hz | 0.01 Hz以上 |
| 時間 | float | [0, 600] 秒 | 約20μs |
| dBレベル | float | [-120, 20] dB | 0.01 dB |
| IACC | float | [0, 1] | 0.001 |
| Bark | float | [0, 24] | 0.01 |

#### バッファアライメント

```c
// SIMD効率のための16バイトアライン
#ifdef _MSC_VER
    #define AE_ALIGN(n) __declspec(align(n))
#else
    #define AE_ALIGN(n) __attribute__((aligned(n)))
#endif

typedef AE_ALIGN(16) struct {
    float samples[AE_MAX_BUFFER_SIZE];
} ae_aligned_buffer_t;

// 確認
_Static_assert(sizeof(ae_aligned_buffer_t) % 16 == 0, 
               "Buffer must be 16-byte aligned");
```

---

### D.5 エッジケースの処理

| 条件 | 処理 | 理由 |
|-----|------|------|
| `distance = 0` | `distance = 0.1` | 0除算回避 |
| `room_size = 0` | RT60 = 0.1秒 | 最小残響 |
| `brightness = NaN` | `brightness = 0` | デフォルト |
| `IACC分母 = 0` | `IACC = 1.0` | 無音は相関的 |
| `C50分母 = 0` | `C50 = +∞ dB` | 直接音のみ |
| `FFT入力がゼロ` | `centroid = 0` | 未定義を回避 |

