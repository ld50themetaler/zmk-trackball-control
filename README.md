# ZMK 汎用高機能トラックボール制御モジュール (`zmk-trackball-control`)

ZMK Firmware 向けの**センサー非依存・高機能トラックボール制御モジュール**です。  
自作キーボード界隈で広く使われている **PMW3610**（torabo-tsuki、Lotom、Keyball、Cocot 等）や **PAW3204**（Bit Trade One ADTB7M、Kugel-1）など、あらゆるポインティングデバイスで「極上の操作感」を実現します。

---

## 🌟 主な機能と特徴

- 🎯 **端数累積バッファ（サブピクセル演算・極低速リニア追従）**:
  - 整数除算による切り捨てノイズや、強制的な1カウント跳ね上がりを解消。
  - 小数の余りを次回サンプリングへキャリーオーバーすることで、極低速域（0.60x等）やスナイパーモードでも1ドット単位で滑らか〜にリニア追従します。
  - 切り返し時（方向転換時）の残存端数を自動クリアし、余分なオーバーシュートを防ぎます。
- ⚡ **連続2次関数スムーズ加速カーブ**:
  - 5段階の加速度プロファイル（Gentle 〜 Ultra）。
  - 低速域での微小精密操作（1カウントで0.6x、2カウントで0.8x）と、高速域での広い画面移動をシームレスに両立します。
- 📐 **動的回転角度補正（10度刻み調整・NVS自動保存）**:
  - キーボード中央や親指位置のトラックボールに手を斜めに伸ばした際の自然な手の向きに合わせて、上下左右の軸を10度刻みで傾き補正可能。
  - キー操作（`TB_ROT_CW`, `TB_ROT_CCW`, `TB_ROT_RES`）でいつでも調整でき、設定値はマイコン内蔵 Flash (NVS) に自動永続化されます。
- 🌊 **ソフトスムージング（EMAフィルタ）**:
  - ボール表面の微小な物理的ひっかかりやマイクロジッターをうっすら抑える指数移動平均フィルタを実装。
  - キー操作（`TB_SMOOTH_TOG`）でいつでも **ON（しっとり重厚感）⇄ OFF（キレとダイレクト感）** をトグル切り替え可能。高速移動時は自動的にバイパスされ遅延ゼロを維持します。
- 📜 **軸ロック付きモーメンタリ・スクロール**:
  - スムーズな垂直・水平スクロールホイール生成。
  - 縦スクロール中の不意な横ブレを防ぐ方向性軸ロックを内蔵。6段階の感度調整に対応。
- 🐭 **タイピング即解除付きオートマウスレイヤー**:
  - ボールを回すと自動的にマウスレイヤーへ遷移。
  - タイムアウト時間（200ms〜3000ms）の動的調整に対応。通常の文字キーを打鍵した瞬間にディレイゼロで即座に解除されます。
- 💾 **設定のNVS自動永続化**:
  - ポインター速度、スクロール感度、回転角度、加速度、スムージングの設定値は内蔵フラッシュに自動保存され、電源を切っても維持されます。

---

## 📦 導入方法 (`west.yml`)

お使いのキーボードリポジトリの `config/west.yml` に本モジュールを追加します：

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: ld50themetaler
      url-base: https://github.com/ld50themetaler
  projects:
    - name: zmk
      remote: zmkfirmware
      ...
    # 本モジュールを追加:
    - name: zmk-trackball-control
      remote: ld50themetaler
      revision: main
```

---

## ⚙️ Kconfig 設定 (`<keyboard>.conf`)

```ini
# トラックボール制御サブシステムの有効化
CONFIG_TRACKBALL_CONTROL=y

# マウスレイヤー番号（お使いの keymap に合わせて変更可能、デフォルト: 4）
CONFIG_TRACKBALL_MOUSE_LAYER_ID=4

# スナイパーレイヤー番号（デフォルト: 5）
CONFIG_TRACKBALL_SNIPE_LAYER_ID=5

# ポインター初期速度（1〜16段階、デフォルト: 8 = 1.00x）
CONFIG_TRACKBALL_DEFAULT_SPEED_LEVEL=8

# スクロール初期感度（1〜6段階、デフォルト: 3）
CONFIG_TRACKBALL_DEFAULT_SCROLL_LEVEL=3

# 回転角度の初期値（度数法、デフォルト: 0）
CONFIG_TRACKBALL_DEFAULT_ROTATION_ANGLE=0
```

---

## ⌨️ キーマップ定義 (`<keyboard>.keymap`)

キーマップファイルでヘッダーをインクルードし、ビヘイビアを定義します：

```dts
#include <dt-bindings/zmk/trackball.h>

/ {
    behaviors {
        tb: behavior_trackball {
            compatible = "zmk,behavior-trackball";
            #binding-cells = <1>;
        };
    };
};
```

### 利用可能な操作コマンド:

| キーコード | 説明 |
| :--- | :--- |
| `&tb TB_SPD_UP` / `&tb TB_SPD_DN` | ポインター速度 アップ / ダウン（16段階） |
| `&tb TB_SCRL_UP` / `&tb TB_SCRL_DN` | スクロール感度 アップ / ダウン（6段階） |
| `&tb TB_SCRL_TOG` / `&tb TB_SCRL_MO` | モーメンタリ・スクロールモード（押している間スクロール） |
| `&tb TB_ACCEL_TOG` | 2次関数加速カーブの ON / OFF トグル |
| `&tb TB_ACCEL_UP` / `&tb TB_ACCEL_DN` | 加速度プロファイル変更（1〜5段階） |
| `&tb TB_SMOOTH_TOG` | ソフトスムージング（EMAフィルタ）の ON / OFF トグル |
| `&tb TB_ROT_CW` / `&tb TB_ROT_CCW` | 動作角度を時計回り / 反時計回りに +10° / -10° 調整 |
| `&tb TB_ROT_RES` | 動作角度を 0°（初期値）にリセット |
| `&tb TB_AM_TOG` | オートマウスレイヤー機能の 有効 / 無効 トグル |
| `&tb TB_AM_TIME_UP` / `DN` / `RES` | オートマウス持続時間調整（+100ms / -100ms / 800msリセット） |

---

## 🔌 センサードライバへの組み込み例 (PMW3610 / PAW3204)

センサーから生の移動カウント $(dx, dy)$ を取得した箇所で、本モジュールの API を呼び出すだけで簡単に極上制御を適用できます：

```c
#include <trackball_control.h>

void on_sensor_data(int raw_dx, int raw_dy) {
    // 1. オートマウス起動フック
    trackball_control_on_motion(raw_dx, raw_dy);

    // 2. 角度補正計算
    int rot_dx = 0, rot_dy = 0;
    trackball_control_rotate_motion(raw_dx, raw_dy, &rot_dx, &rot_dy);

    // 3. スクロールモード判定
    if (trackball_control_is_scroll_mode()) {
        // スクロール除数テーブルを使用してホイールイベントを報告
    } else {
        // 4. スムージング、2次関数加速、端数累積バッファによる高精度計算
        int final_dx = 0, final_dy = 0;
        trackball_control_calculate_motion(rot_dx, rot_dy, &final_dx, &final_dy);

        // 5. HID イベント報告（端数蓄積中の 0 移動は送信スキップ）
        if (final_dx != 0 || final_dy != 0) {
            input_report_rel(dev, INPUT_REL_X, final_dx, false, K_NO_WAIT);
            input_report_rel(dev, INPUT_REL_Y, final_dy, true, K_NO_WAIT);
        }
    }
}
```

---

## 💡 ボード固有のLED・インジケーターとの連携

本モジュールはハードウェア非依存（特定のLEDピンやドライバに依存しない）設計になっています。
スムージングのON/OFF切り替え時にLEDを点滅させたい場合などは、キーボード側のコードで以下のコールバック関数を実装するだけで自動的にフックされます（未定義時は何もしない Weak Symbol となっています）：

```c
#include <trackball_control.h>

// スムージング切り替え時に自動的に呼び出されるコールバック
void trackball_control_on_smoothing_toggled(bool enabled) {
    if (enabled) {
        // 例: ON の場合は2回点滅、あるいはLED点灯
    } else {
        // 例: OFF の場合は1回点滅、あるいはLED消灯
    }
}
```

---

## 📄 ライセンス

MIT License - 詳細は [LICENSE](LICENSE) ファイルをご確認ください。
