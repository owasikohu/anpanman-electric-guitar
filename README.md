# Raspberry Pi Pico ADC USB microphone

RP2040 の GPIO26 / ADC0 を、USB Audio Class 2 (UAC2) のモノラルマイクとして公開する最小プロジェクトです。音声形式は 48 kHz、16-bit signed PCM、1 channel 固定です。USB descriptor は TinyUSB 公式の `TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR` を利用しています。

## ハードウェア

- Raspberry Pi Pico (RP2040)
- 音声入力: GPIO26 / ADC0
- 入力信号は外部回路で約 1.65 V にバイアスしてください
- ADC の基準/GND は Pico の ADC_VREF / AGND を使用してください
- GPIO26 の絶対最大定格を超える電圧を入力しないでください

ADC の 12-bit 値から 2048 を引き、4 bit 左シフトして signed 16-bit PCM にします。ゲイン、フィルタ、自動音量調整は行いません。

## ビルド

Pico SDK、TinyUSB、ARM GNU toolchainを準備してください。Pico SDK 2.3.0同梱のTinyUSB 0.18.0にはRP2040のUAC2 isochronous endpoint開始時に停止する問題があるため、このプロジェクトでは修正済みTinyUSB 0.21以降を `PICO_TINYUSB_PATH` で指定します。

```sh
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH="$PWD"
cd ..
git clone https://github.com/hathach/tinyusb.git
export PICO_TINYUSB_PATH="$PWD/tinyusb"
cd /path/to/anpanman-electric-guitar
cmake -S . -B build -DPICO_BOARD=pico \
  -DPICO_TINYUSB_PATH="$PICO_TINYUSB_PATH"
cmake --build build -j
```

動作確認済みのTinyUSB commitは `84dcec4`（version 0.21.0）です。再現性を固定したい場合はTinyUSB側でこのcommitをcheckoutしてください。

生成物は `build/pico_usb_microphone.uf2` です。

## UF2 の書き込み

1. Pico の BOOTSEL ボタンを押したまま USB 接続します。
2. `RPI-RP2` として現れたドライブへ `build/pico_usb_microphone.uf2` をコピーします。
3. Pico が自動再起動したら、一度 USB を抜き差しします。

## Linux で確認・録音

認識を確認します。

```sh
arecord -l
arecord --dump-hw-params -D hw:CARD=Microphone,DEV=0 /dev/null
```

カード名が環境によって異なる場合は、`arecord -l` に表示された card 番号を使います（以下は card 2 の例）。

```sh
arecord -D hw:2,0 -f S16_LE -r 48000 -c 1 -d 10 recording.wav
aplay recording.wav
```

PipeWire/PulseAudio の既定入力として選ぶ場合も、`Pico ADC Microphone` という名前で表示されます。

## サンプルレートと実装上の注意

RP2040 の ADC クロックは 48 MHz です。`adc_set_clkdiv(1000.0f)` により変換開始間隔を 1000 ADC clock とし、`48,000,000 / 1000 = 48,000 sample/s` としています。ADC の最小変換時間 96 clock より十分長い設定です。ADC FIFO の DREQ で DMA を pacing し、48 sample単位のブロックを16個持つリングバッファへ連続転送します。

USB full-speed の 1 ms frameごとの通常転送量は、48 sample x 2 byte x 1 channel = 96 byte です。TinyUSB 公式の `TUD_AUDIO_EP_SIZE` は非同期 endpoint のクロック差を許容する 1 sample 分を加えるため、descriptor の `wMaxPacketSize` は 98 byte になります。TinyUSB の sample-rate control と ADC の実効レートはどちらも 48 kHz です。

この最小版の DC 除去は、1.65 V が ADC 中点（code 2048）である前提の固定オフセット減算です。実回路のオフセット誤差まで追従するハイパスフィルタは入れていません。
