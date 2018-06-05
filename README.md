# keymodify2

Arduino Leonardo と USBホストシールドを使って入力されたキーを変換してPCに中継します。

- USBホストシールドの仕様に基づいて入力されたキー情報を取得します。
https://felis.github.io/USB_Host_Shield_2.0/class_keyboard_report_parser.html#aa21e9230ae4c2f068c404a9c2909a1f3

- ArduinoをHIDキーボードとしてPCに認識させることでキー送信を行います。
https://www.arduino.cc/en/Reference/KeyboardPress

内容の多くを、 https://github.com/Tamakichi/Arduino_USBToPS2 をもとにさせていただいています。
