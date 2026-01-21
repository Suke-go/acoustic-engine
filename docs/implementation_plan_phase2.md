# Acoustic Engine 残機能実装計画

## 概要

Acoustic Engineの残り4つの主要機能を実装します：
1. **DRNL フィルタバンク** - 蝸牛の非線形圧縮モデル
2. **変調フィルタバンク** - 時間エンベロープ解析 (0.5-256Hz)
3. **SLM ONNX統合** - 自然言語インターフェース
4. **プラグインラッパー** - Unity/VST3/AU統合

---

## User Review Required

> [!IMPORTANT]
> **Phase 2-3 (SLM ONNX / プラグインラッパー) について確認が必要です:**
> - ONNX Runtimeを依存関係に追加してよいですか？（ビルドサイズ増加）
> - VST3/AU実装にはSDKのライセンス確認が必要です
> - Unity C#バインディングはp4g生成ツールを使用しますか？

> [!WARNING]
> **スコープに関する推奨:**
> Phase 1 (DRNL + 変調フィルタバンク) は純粋なC実装で外部依存なし。
> Phase 2-3は外部依存が複雑なため、Phase 1のみ先行実装を推奨します。

---

## Proposed Changes

### Phase 1: 聴覚モデリング拡張

---

#### [MODIFY] [acoustic_engine.h](file:///c:/Users/kosuk/acoustic_engine/include/acoustic_engine.h)

**DRNL API追加:**
```c
/* DRNL Configuration */
typedef struct {
    uint32_t n_channels;
    float f_low;
    float f_high;
    float compression_exp;  /* 約0.25 */
    uint32_t sample_rate;
} ae_drnl_config_t;

typedef struct ae_drnl ae_drnl_t;

AE_API ae_drnl_t* ae_drnl_create(const ae_drnl_config_t* config);
AE_API void ae_drnl_destroy(ae_drnl_t* drnl);
AE_API ae_result_t ae_drnl_process(ae_drnl_t* drnl, const float* input,
                                    size_t n_samples, float** output);
```

**変調フィルタバンク API追加:**
```c
/* Modulation Filterbank Configuration */
typedef struct {
    uint32_t n_channels;     /* 8-16 typical */
    float f_low;             /* 0.5 Hz */
    float f_high;            /* 256 Hz */
    uint32_t sample_rate;
} ae_modfb_config_t;

typedef struct ae_modfb ae_modfb_t;

AE_API ae_modfb_t* ae_modfb_create(const ae_modfb_config_t* config);
AE_API void ae_modfb_destroy(ae_modfb_t* mfb);
AE_API ae_result_t ae_modfb_process(ae_modfb_t* mfb, const float* input,
                                     size_t n_samples, float** output);
```

---

#### [NEW] [ae_drnl.c](file:///c:/Users/kosuk/acoustic_engine/src/ae_drnl.c)

DRNL (Dual-Resonance Nonlinear) フィルタバンク実装:

```
構造:
  入力 ─┬─→ [線形パス] ──────────────────────────┬─→ 出力
        │    GT → LP → Gain                      │
        │                                         │
        └─→ [非線形パス] ────────────────────────┘
             GT → LP → Compression → GT → LP

Broken-stick圧縮:
  y = sign(x) × min(a|x|, b|x|^c)  where c ≈ 0.25
```

---

#### [NEW] [ae_modfb.c](file:///c:/Users/kosuk/acoustic_engine/src/ae_modfb.c)

変調フィルタバンク実装:

```
周波数配置: 対数間隔 0.5Hz - 256Hz
典型的なチャンネル: [0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256] Hz

Q設定:
  f < 10Hz: Q = 1 (低周波は広帯域)
  f ≥ 10Hz: Q = 2 (高周波は狭帯域)

用途: Roughness (70Hz付近), Fluctuation (4Hz付近) 検出
```

---

#### [MODIFY] [CMakeLists.txt](file:///c:/Users/kosuk/acoustic_engine/CMakeLists.txt)

新しいソースファイルを追加:
```cmake
add_library(acoustic_engine SHARED
    # ... existing files ...
    src/ae_drnl.c
    src/ae_modfb.c
)
```

---

#### [NEW] [test_auditory.c 追加テスト](file:///c:/Users/kosuk/acoustic_engine/tests/test_auditory.c)

```c
/* DRNL compression characteristic test */
void test_drnl_compression(void);

/* Modulation filterbank 70Hz detection test */
void test_modfb_modulation_detection(void);
```

---

### Phase 2: SLM ONNX統合 (オプショナル)

> [!CAUTION]
> ONNX Runtimeは大きな依存関係です（数十MB）。
> ユーザー確認後に実装を進めます。

#### [NEW] [ae_slm_onnx.c](file:///c:/Users/kosuk/acoustic_engine/src/ae_slm_onnx.c)

```c
/* ONNX-based SLM inference */
AE_API ae_result_t ae_slm_load_model(const char* model_path);
AE_API ae_result_t ae_slm_interpret_natural(const char* text,
                                             ae_main_params_t* out_params);
AE_API void ae_slm_unload_model(void);
```

---

### Phase 3: プラグインラッパー (オプショナル)

> [!CAUTION]
> VST3/AU SDKのライセンス確認が必要です。
> ユーザー確認後に実装を進めます。

#### [NEW] [bindings/unity/AcousticEngine.cs](file:///c:/Users/kosuk/acoustic_engine/bindings/unity/AcousticEngine.cs)

Unity C# P/Invokeバインディング

#### [NEW] [plugins/vst3/](file:///c:/Users/kosuk/acoustic_engine/plugins/vst3/)

VST3プラグインラッパー

#### [NEW] [plugins/au/](file:///c:/Users/kosuk/acoustic_engine/plugins/au/)

Audio Unitプラグインラッパー

---

## Verification Plan

### Automated Tests

**Phase 1 テスト:**

```powershell
# ビルド
cd c:\Users\kosuk\acoustic_engine
cmake --build build --config Release

# 全テスト実行
.\build\Release\test_auditory.exe
```

**期待結果:**
- `test_drnl_compression`: 入力レベル変化に対して0.25乗圧縮が適用される
- `test_modfb_modulation_detection`: 70Hz AM信号で70Hz帯域が最大出力

### Manual Verification

1. **DRNL圧縮特性確認:**
   - 入力レベルを-60dB～0dBで変化
   - 出力レベル変化が入力の約1/4であることを確認

2. **変調フィルタバンク確認:**
   - 4Hz AM信号 → 4Hz帯域が最大
   - 70Hz AM信号 → 70Hz帯域が最大

---

## 推奨実装順序

| 順序 | 機能 | 工数 | 依存関係 |
|-----|------|------|---------|
| 1 | DRNL | 2時間 | なし |
| 2 | 変調フィルタバンク | 2時間 | なし |
| 3 | テスト追加 | 1時間 | 1, 2 |
| 4 | (オプション) Unity C# | 2時間 | DLL完成 |
| 5 | (オプション) VST3 | 5時間 | SDK確認 |
| 6 | (オプション) ONNX SLM | 3時間 | ONNX Runtime |

**推奨:** Phase 1のみ先行実装し、Phase 2-3は要件確認後に実装
