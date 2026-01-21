# Acoustic Engine - 開発進捗ドキュメント

> **最終更新**: 2026-01-21  
> **バージョン**: 0.1.0

---

## 概要

このドキュメントは、Acoustic Engine の開発進捗と実装状況を記録します。新規参加者がプロジェクトの現状を把握し、開発に参加するための参考資料です。

---

## 実装状況

### 全体進捗

| フェーズ | 状態 | 完了率 |
|---------|------|--------|
| Phase 1: コア機能 | ✅ 完了 | 100% |
| Phase 2: 拡張機能 | ✅ 完了 | 100% |
| Phase 3: 最適化 | 🔄 進行中 | 80% |
| Phase 4: ドキュメント | 🔄 進行中 | 60% |

### モジュール別実装状況

#### コアモジュール

| モジュール | ファイル | 状態 | 備考 |
|-----------|---------|------|------|
| エンジン本体 | `acoustic_engine.c` | ✅ 完了 | 基本API実装完了 |
| オーディオI/O | `ae_audio_io.c` | ✅ 完了 | WAV読み込み対応 |
| プリセット管理 | `ae_presets.c` | ✅ 完了 | シナリオプリセット |
| 数学ユーティリティ | `ae_math.c` | ✅ 完了 | Phon/Sone変換等 |

#### DSP モジュール

| モジュール | ファイル | 状態 | 備考 |
|-----------|---------|------|------|
| DSP基本演算 | `ae_dsp.c` | ✅ 完了 | Biquad, FFT等 |
| SIMD最適化 | `ae_simd.c` | ✅ 完了 | SSE2対応 |
| リバーブ (FDN) | `ae_reverb.c` | ✅ 完了 | 8ch FDN, Hadamard |
| 空間処理 | `ae_spatial.c` | ✅ 完了 | HRTF, M/S, デコリレーション |

#### 物理モデル

| モジュール | ファイル | 状態 | 備考 |
|-----------|---------|------|------|
| 音響伝播 | `ae_propagation.c` | ✅ 完了 | Francois-Garrison, ISO 9613-1, 洞窟モデル |

#### ダイナミクス処理

| モジュール | ファイル | 状態 | 備考 |
|-----------|---------|------|------|
| ダイナミクス | `ae_dynamics.c` | ✅ 完了 | コンプレッサー, リミッター, デエッサー |

#### 分析モジュール

| モジュール | ファイル | 状態 | 備考 |
|-----------|---------|------|------|
| 知覚メトリクス | `ae_analysis.c` | ✅ 完了 | IACC, DRR, ラウドネス等 |

#### 将来拡張

| モジュール | ファイル | 状態 | 備考 |
|-----------|---------|------|------|
| SLM (自然言語) | `ae_slm.c` | 🔄 基本実装 | 将来のLLM統合用スタブ |

---

## テスト状況

### 自動テスト

| テストファイル | 対象 | 状態 | テスト数 |
|--------------|------|------|---------|
| `test_math.c` | 数学ユーティリティ | ✅ Pass | 15 |
| `test_dsp.c` | DSP基本演算 | ✅ Pass | 22 |
| `test_analysis.c` | 分析機能 | ✅ Pass | 19 |

**合計**: 56テスト (100% Pass)

### テストの実行

```bash
cd build
ctest --output-on-failure
```

または Windows の場合:

```powershell
.\run_tests.ps1
```

---

## 開発環境のセットアップ

### 必要なツール

1. **CMake** 3.16以上
2. **C11対応コンパイラ**
   - Windows: Visual Studio 2019以上
   - Linux: GCC 7以上
   - macOS: Clang 5以上
3. **Git**

### ビルド手順

#### Windows (Visual Studio)

```powershell
# リポジトリをクローン
git clone https://github.com/your-org/acoustic_engine.git
cd acoustic_engine

# ビルドディレクトリを作成
mkdir build
cd build

# CMake設定
cmake .. -G "Visual Studio 17 2022" -A x64

# ビルド
cmake --build . --config Release

# テスト実行
ctest --output-on-failure
```

#### Linux / macOS

```bash
git clone https://github.com/your-org/acoustic_engine.git
cd acoustic_engine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest --output-on-failure
```

### デバッグビルド

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

---

## アーキテクチャ概要

### レイヤー構造

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                         │
│  (ユーザーアプリケーション)                                        │
├─────────────────────────────────────────────────────────────────┤
│                         Public API                               │
│  acoustic_engine.h                                               │
│  - ae_create_engine() / ae_destroy_engine()                     │
│  - ae_process() / ae_apply_scenario()                           │
│  - ae_set_*() / ae_get_*()                                      │
├─────────────────────────────────────────────────────────────────┤
│                      Processing Modules                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐       │
│  │ Reverb   │ │ Spatial  │ │ Dynamics │ │ Propagation  │       │
│  │(ae_reverb)│ │(ae_spatial)│ │(ae_dynamics)│ │(ae_propagation)│       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────┘       │
├─────────────────────────────────────────────────────────────────┤
│                        DSP Primitives                            │
│  ae_dsp.c (Biquad, FFT, Delay)                                  │
│  ae_simd.c (SSE2/AVX2最適化)                                     │
│  ae_math.c (安全な数学関数)                                       │
├─────────────────────────────────────────────────────────────────┤
│                          Analysis                                │
│  ae_analysis.c (IACC, DRR, Loudness, Spectral)                  │
└─────────────────────────────────────────────────────────────────┘
```

### 処理フロー

```
入力バッファ
    │
    ▼
┌─────────────────┐
│ Propagation     │ ← 距離減衰、吸収
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Spatial         │ ← HRTF、パンニング
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Reverb          │ ← FDN、Early Reflections
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Dynamics        │ ← コンプレッサー、リミッター
└────────┬────────┘
         │
         ▼
出力バッファ
```

---

## コーディング規約

### 命名規則

| 種類 | 規則 | 例 |
|-----|------|-----|
| 関数 (Public) | `ae_` プレフィックス + スネークケース | `ae_create_engine()` |
| 関数 (Internal) | `ae_internal_` プレフィックス | `ae_internal_apply_filter()` |
| 型定義 | `ae_` + 名前 + `_t` | `ae_engine_t` |
| 列挙値 | `AE_` + 大文字スネークケース | `AE_OK`, `AE_ERROR_INVALID_PARAM` |
| マクロ | `AE_` + 大文字スネークケース | `AE_SAMPLE_RATE` |

### ファイル構成

- **ヘッダーファイル**: `include/` に public API のみ
- **ソースファイル**: `src/` にすべての実装
- **内部ヘッダー**: `src/ae_internal.h` に internal 宣言

### エラーハンドリング

```c
ae_result_t ae_some_function(ae_engine_t* engine, float value) {
    // パラメータ検証
    if (!engine) return AE_ERROR_INVALID_PARAM;
    if (value < 0.0f || value > 1.0f) return AE_ERROR_INVALID_PARAM;
    
    // 処理...
    
    return AE_OK;
}
```

---

## 今後の開発予定

### 短期 (v0.2.0)

- [ ] AVX2最適化の追加
- [ ] Lo-Fi処理の拡張 (Wow/Flutter, テープサチュレーション)
- [ ] マルチバンドコンプレッサー
- [ ] SOFA HRTFの完全統合

### 中期 (v0.3.0)

- [ ] シナリオComposerの完全実装
- [ ] 生体信号マッピングの実装
- [ ] WebAssembly (Emscripten) 対応
- [ ] Python バインディング

### 長期 (v1.0.0)

- [ ] SLM (Small Language Model) 統合
- [ ] リアルタイムレイトレーシングリバーブ
- [ ] GPU (Vulkan Compute) 対応
- [ ] 完全なドキュメントとチュートリアル

---

## 既知の問題

| Issue | 状態 | 優先度 | 備考 |
|-------|------|--------|------|
| 大きなバッファサイズでのメモリ使用量 | 調査中 | 中 | 4096サンプル以上で顕著 |
| HRTF切り替え時のポップノイズ | 回避策あり | 低 | クロスフェード長を増加で軽減 |

---

## 参考資料

### 主要な論文・規格

- **Francois-Garrison (1982)**: Sound absorption based on ocean measurements
- **ISO 9613-1:1993**: Acoustics - Attenuation of sound during propagation outdoors
- **ISO 3382**: Acoustics - Measurement of room acoustic parameters
- **EBU R128**: Loudness normalisation and permitted maximum level of audio signals

### 依存ライブラリ

| ライブラリ | バージョン | ライセンス | 用途 |
|-----------|-----------|-----------|------|
| libmysofa | 1.3.2 | BSD-3 | SOFA HRTF読み込み |
| zlib | 1.3.1 | zlib | libmysofa依存 |

---

## 連絡先

- **開発リーダー**: (TBD)
- **Issue Tracker**: (リポジトリURL)/issues
- **Discussion**: (リポジトリURL)/discussions
