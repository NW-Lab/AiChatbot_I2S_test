# AiChatbot_I2S_test

M5Stack AiChatBot (K147 / AtomS3R + Atomic Voice Base) 用のArduino I2Sサンプルです。

## I2S_SineScale

中央ボタンを押すたびに、C4からC5までのド・レ・ミ・ファ・ソ・ラ・シ・ドを1音ずつサイン波で再生します。最後のドの次はC4に戻ります。

### 準備

- Arduino IDEでESP32-S3用ボード（AtomS3R）を選択します。
- `M5Unified` と `M5Atomic-EchoBase` ライブラリをインストールします。
- `examples/I2S_SineScale/I2S_SineScale.ino` を開いて書き込みます。
- このスケッチはK147のAtomic Voice Baseを対象とし、I2C SDA/SCLとI2S DIN/WS/DOUT/BCKにGPIO 38/39/7/6/5/8を使用します。

音量はスケッチ内の `setSpeakerVolume(30)` で調整できます。
