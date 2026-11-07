# AiChatbot_I2S_test

M5Stack AiChatBot (K147 / AtomS3R + Atomic Voice Base) 用のArduino I2Sサンプルです。

## I2S_SineScale

中央ボタンを押すたびに、C4からC5までのド・レ・ミ・ファ・ソ・ラ・シ・ドを1音ずつサイン波で再生します。最後のドの次はC4に戻ります。

### 準備

- Arduino IDEでESP32-S3用ボード（AtomS3R）を選択します。
- `M5Unified` と `M5Atomic-EchoBase` ライブラリをインストールします（`M5Atomic-EchoBase` 1.0.2で確認）。
- `examples/I2S_SineScale/I2S_SineScale.ino` を開いて書き込みます。
- このスケッチはK147のAtomic Voice Baseを対象とし、I2C SDA/SCLとI2S WS/DOUT/BCKにGPIO 38/39/6/5/8を使用します。マイク入力は使用しません。

音量はスケッチ内の `SPEAKER_VOLUME`（0〜100、初期値75）で調整できます。ES8311の音量設定は聴感上の割合と一致しないため、30ではほとんど聞こえません。大きくする場合は少しずつ調整してください。

起動時にES8311とスピーカーアンプの設定、およびI2Sクロックを確認し、結果をシリアルモニター（115200 baud）に表示します。ボタンを押すと、再生する音名とI2Sに書き込んだバイト数が表示されます。エラーが出る場合は、AtomS3RとAtomic Voice Baseの接続を確認してください。バイト数の表示は実際に音が出たことまでは保証しません。

`M5Atomic-EchoBase` 1.0.2のES8311ドライバーを使用し、I2Sの開始・書き込み結果を確認できるように、このサンプルでI2S出力を初期化します。コーデックとI2Sのデータ幅を16ビットに揃えてクロックを設定し、コーデックとアンプの両方のミュートを解除します。
