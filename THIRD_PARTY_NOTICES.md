# 使用ソースとライセンス

ゲームの絵は `src/platform/render.cpp` の図形描画による独自デザインです。キャラクター画像、音声、任天堂作品の素材、crisp-game-libのコードは取り込んでいません。効果音はブザーの短い単音です。

| 使用物 | バージョン・出典 | ライセンス・同梱通知 |
| --- | --- | --- |
| SE公式Factory（設定・確認用ソース） | M5Stack `M5StickC-Plus-SE-Factory`、ZIPコミット1b24c439 | MIT、`docs/reference/FACTORY_LICENSE`。Factoryのゲーム外機能（BLE/Wi-Fi/IR/FFT）は取り込まない |
| M5Unified | 0.2.23、https://github.com/m5stack/M5Unified | MIT、`lib/M5Unified/LICENSE`。固定したローカルキャッシュのソースを同梱 |
| M5GFX | 0.2.30、https://github.com/m5stack/M5GFX | MIT、`lib/M5GFX/LICENSE`。内部の個別ライセンスは同梱README・各ソースの通知を保持 |
| LovyanGFX（M5GFX内部） | https://github.com/lovyan03/LovyanGFX | FreeBSD / BSD-2-Clause、ソースの著作権・出典通知を保持 |
| IPAゴシック変換フォント | M5GFX `lgfxJapanGothic_16` / `_24`、https://moji.or.jp/ipafont/ | IPA Font License v1.0、`lib/M5GFX/src/lgfx/Fonts/IPA/IPA_Font_License_Agreement_v1.0.txt`。変換方法は同ディレクトリのREADME、変換後データと元フォントへの参照を保持 |
| 数字Font4など（M5GFX内部） | TFT_eSPI / Bodmer、https://github.com/Bodmer/TFT_eSPI | FreeBSD。M5GFXのREADMEに出典、元のデータを同梱 |
| efont（同梱ライブラリ内、アプリ画面には不使用） | The Electronic Font Open Laboratory | BSD-3-Clause、`lib/M5GFX/src/lgfx/Fonts/efont/COPYRIGHT.txt` |
| その他の同梱M5GFXフォント | Adafruit GFX / TomThumb等 | `lib/M5GFX/README.md`、`src/lgfx/Fonts/GFXFF/license.txt`、各フォント内通知 |
| Arduino-ESP32 / Preferences | 2.0.17、https://github.com/espressif/arduino-esp32 | LGPL-2.1、FrameworkのLICENSEを参照。PlatformIO取得物であり、このプロジェクトの `lib/` には複製しない |
| PlatformIO Espressif32 | 6.12.0、https://github.com/platformio/platform-espressif32 | Apache-2.0 |
| SDL2（PC描画検証のみ） | インストール済み開発環境、https://libsdl.org/ | zlib。ファームウェアには含まない |

同梱ライブラリはライセンス通知とソースを保持しています。ファームウェアを再配布する場合も、この通知・関連する同梱ライセンス・再ビルドに必要なソースへのアクセスを一緒に提供してください。
