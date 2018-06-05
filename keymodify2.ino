//******* 用途により、下記の定義を設定して下さい ***************************
#define MYDEBUG      1  // 0:デバッグ情報出力なし 1:デバッグ情報出力あり 
#define KB_CLK      A4  // PS/2 CLK  IchigoJamのKBD1に接続
#define KB_DATA     A5  // PS/2 DATA IchigoJamのKBD2に接続
//**************************************************************************

#include <MsTimer2.h>
#include <Usb.h>
#include <usbhub.h>
#include <hidboot.h>
#include "Keyboard.h"

#define LOBYTE(x) ((char*)(&(x)))[0]
#define HIBYTE(x) ((char*)(&(x)))[1]

// キーリピートの定義
#define REPEATTIME      5   // キーを押し続けて、REP_INTERVALxREPEATTIMEmsec後にリピート開始
#define EMPTY           0   // リピート管理テーブルが空状態
#define MAXKEYENTRY     6   // リピート管理テーブルサイズ
#define REP_INTERVAL    100 // リピート間隔 150msec

#define MS_SIKIICHI     10

uint8_t keyentry[MAXKEYENTRY];    // リピート管理テーブル
uint8_t repeatWait[MAXKEYENTRY];  // リピート開始待ち管理テーブル
uint8_t enabled = 0;              // PS/2 ホスト送信可能状態

//
// HIDキーボード レポートパーサークラスの定義
//
class KbdRptParser : public KeyboardReportParser {
  protected:
    virtual uint8_t HandleLockingKeys(USBHID *hid, uint8_t key);
    virtual void OnControlKeysChanged(uint8_t before, uint8_t after);
    virtual void OnKeyDown(uint8_t mod, uint8_t key);
    virtual void OnKeyUp(uint8_t mod, uint8_t key);
    virtual void OnKeyPressed(uint8_t key) {};
};

USB     Usb;
USBHub  Hub1(&Usb);

KbdRptParser keyboardPrs;
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    HidKeyboard(&Usb);

uint8_t classType = 0;
uint8_t subClassType = 0;
uint32_t next_time;



// リピート管理テーブルのクリア
void claerKeyEntry() {
  for (uint8_t i = 0; i < MAXKEYENTRY; i++)
    keyentry[i] = EMPTY;
}

// リピート管理テーブルにキーを追加
void addKey(uint8_t key) {
  for (uint8_t i = 0; i < MAXKEYENTRY; i++) {
    if (keyentry[i] == EMPTY) {
      keyentry[i] = key;
      repeatWait[i] = REPEATTIME;
      break;
    }
  }
}

// リピート管理テーブルからキーを削除
void delKey(uint8_t key) {
  for (uint8_t i = 0; i < MAXKEYENTRY; i++) {
    if (keyentry[i] == key) {
      keyentry[i] = EMPTY;
      break;
    }
  }
}

//
// PS/2 breakコード送信
// 引数 key(IN) HID Usage ID
//
uint8_t sendKeyBreak(uint8_t key) {
  Keyboard.release(changeKeyCode(key));
  return changeKeyCode(key);
}

uint8_t sendKeyMake(uint8_t key) {
  Keyboard.press(changeKeyCode(key));
  return changeKeyCode(key);
}

uint8_t changeKeyCode(uint8_t key) {
  char charMapArray[] = {
    'N', 'N', 'N', 'N', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
    'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    KEY_RETURN, KEY_ESC, KEY_BACKSPACE, KEY_TAB, ' ', '-', '=', '[', ']', 'N',
    'N', ';', '\'', 'N', ',', '.', '/', 'N', KEY_F1, KEY_F2,
    KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    'N', 'N', 'N', KEY_INSERT, KEY_HOME, KEY_PAGE_UP, KEY_DELETE, KEY_END, KEY_PAGE_DOWN, KEY_RIGHT_ARROW,
    KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_UP_ARROW, 'N', 'N', 'N', 'N', 'N', 'N', 'N',
    'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N',
    'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N',
    'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N',
    'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N',
    'N', 'N', 'N', 'N', 'N', '`', 'N', '\\', 'N', 'N',
    'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N',
    'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N'
  };

  return charMapArray[key];
}

// リピート処理(タイマー割り込み処理から呼ばれる)
void sendRepeat() {
  // HID Usage ID から PS/2 スキャンコード に変換
  uint8_t code = 0;
  uint8_t pre, key;

  for (uint8_t i = 0; i < MAXKEYENTRY; i++) {
    if (keyentry[i] != EMPTY) {
      key = keyentry[i];
      if (repeatWait[i] == 0) {
        //sendKeyMake(key);
        sendKeyBreak(key);
      } else {
        repeatWait[i]--;
      }
    }
  }
}

//
// ロックキー（NumLock/CAPSLock/ScrollLock)ハンドラ
//

uint8_t KbdRptParser::HandleLockingKeys(USBHID *hid, uint8_t key) {
  if (classType == USB_CLASS_WIRELESS_CTRL) {
    uint8_t old_keys = kbdLockingKeys.bLeds;
    switch (key) {
      case UHS_HID_BOOT_KEY_NUM_LOCK:
        kbdLockingKeys.kbdLeds.bmNumLock = ~kbdLockingKeys.kbdLeds.bmNumLock;
        break;
      case UHS_HID_BOOT_KEY_CAPS_LOCK:
        kbdLockingKeys.kbdLeds.bmCapsLock = ~kbdLockingKeys.kbdLeds.bmCapsLock;
        break;
      case UHS_HID_BOOT_KEY_SCROLL_LOCK:
        kbdLockingKeys.kbdLeds.bmScrollLock = ~kbdLockingKeys.kbdLeds.bmScrollLock;
        break;
    }
  } else {
    return KeyboardReportParser::HandleLockingKeys(hid, key);
  }
  return 0;
}


//
// キー押しハンドラ
// 引数
//  mod : コントロールキー状態
//  key : HID Usage ID
//
void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key) {
  MsTimer2::stop();
#if MYDEBUG==1
  Serial.print(F("DN ["));  Serial.print(F("mod="));  Serial.print(mod, HEX);
  Serial.print(F(" key="));  Serial.print(key, HEX);  Serial.println(F("]"));
#endif
  if (sendKeyMake(key))
    addKey(key);
  MsTimer2::start();
}

//
// キー離し ハンドラ
// 引数
//  mod : コントロールキー状態
//  key : HID Usage ID
//
void KbdRptParser::OnKeyUp(uint8_t mod, uint8_t key) {
  MsTimer2::stop();
#if MYDEBUG==1
  Serial.print(F("UP ["));  Serial.print(F("mod="));  Serial.print(mod, HEX);
  Serial.print(F(" key="));  Serial.print(key, HEX);  Serial.println(F("]"));
#endif
  if (sendKeyBreak(key)) // HID Usage ID から PS/2 スキャンコード に変換
    delKey(key);
  MsTimer2::start();
}

//
// コントロールキー変更ハンドラ
// SHIFT、CTRL、ALT、GUI(Win)キーの処理を行う
// 引数 before : 変化前のコード USB Keyboard Reportの1バイト目
//      after  : 変化後のコード USB Keyboard Reportの1バイト目
//
void KbdRptParser::OnControlKeysChanged(uint8_t before, uint8_t after) {
  MODIFIERKEYS beforeMod;
  *((uint8_t*)&beforeMod) = before;

  MODIFIERKEYS afterMod;
  *((uint8_t*)&afterMod) = after;

  // 左Ctrlキー
  if (beforeMod.bmLeftCtrl != afterMod.bmLeftCtrl) {
    if (afterMod.bmLeftCtrl) {
      // 左Ctrlキーを押した
      Keyboard.press(KEY_LEFT_CTRL);
    } else {
      // 左Ctrltキーを離した
      Keyboard.release(KEY_LEFT_CTRL);
    }
  }

  // 左Shiftキー
  if (beforeMod.bmLeftShift != afterMod.bmLeftShift) {
    if (afterMod.bmLeftShift) {
      // 左Shiftキーを押した
      Keyboard.press(KEY_LEFT_SHIFT);
    } else {
      // 左Shiftキーを離した
      Keyboard.release(KEY_LEFT_SHIFT);
    }
  }

  // 左Altキー
  if (beforeMod.bmLeftAlt != afterMod.bmLeftAlt) {
    if (afterMod.bmLeftAlt) {
      // 左Altキーを押した
      Keyboard.press(KEY_LEFT_ALT);
    } else {
      // 左Altキーを離した
      Keyboard.release(KEY_LEFT_ALT);
    }
  }

  // 左GUIキー(Winキー)
  if (beforeMod.bmLeftGUI != afterMod.bmLeftGUI) {
    if (afterMod.bmLeftGUI) {
      // 左GUIキーを押した
      Keyboard.press(KEY_LEFT_GUI);
    } else {
      // 左GUIキーを離した
      Keyboard.release(KEY_LEFT_GUI);
    }
  }

  // 右Ctrlキー
  if (beforeMod.bmRightCtrl != afterMod.bmRightCtrl) {
    if (afterMod.bmRightCtrl) {
      // 右Ctrlキーを押した
      Keyboard.press(KEY_RIGHT_CTRL);
    } else {
      // 右Ctrlキーを離した
      Keyboard.release(KEY_RIGHT_CTRL);
    }
  }

  // 右Shiftキー
  if (beforeMod.bmRightShift != afterMod.bmRightShift) {
    if (afterMod.bmRightShift) {
      // 右Shiftキーを押した
      Keyboard.press(KEY_RIGHT_SHIFT);
    } else {
      // 右Shiftキーを離した
      Keyboard.release(KEY_RIGHT_SHIFT);
    }
  }

  // 右Altキー
  if (beforeMod.bmRightAlt != afterMod.bmRightAlt) {
    if (afterMod.bmRightAlt) {
      // 右Altキーを押した
      Keyboard.press(KEY_RIGHT_ALT);
    } else {
      // 右Altキーを離した
      Keyboard.release(KEY_RIGHT_ALT);
    };
  }

  // 右GUIキー
  if (beforeMod.bmRightGUI != afterMod.bmRightGUI) {
    if (afterMod.bmRightGUI) {
      // 右GUIキーを押した
      Keyboard.press(KEY_RIGHT_GUI);
    } else {
      // 右GUIキーを離した
      Keyboard.release(KEY_RIGHT_GUI);
    }
  }
}

void setup() {
  Keyboard.begin();

  Usb.Init();
  delay(200);

  HidKeyboard.SetReportParser(0, (KbdRptParser*)&keyboardPrs);

  claerKeyEntry();
  MsTimer2::set(REP_INTERVAL, sendRepeat);
  MsTimer2::start();
}

void loop() {
  Usb.Task();
}


