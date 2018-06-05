//******* 用途により、下記の定義を設定して下さい ***************************
#define MYDEBUG      1  // 0:デバッグ情報出力なし 1:デバッグ情報出力あり 
//**************************************************************************

#include <MsTimer2.h>
#include <Usb.h>
#include <usbhub.h>
#include <hidboot.h>
#include "Keyboard.h"

// キーリピートの定義
#define REPEATTIME      5   // キーを押し続けて、REP_INTERVALxREPEATTIMEmsec後にリピート開始
#define EMPTY           0   // リピート管理テーブルが空状態
#define MAXKEYENTRY     6   // リピート管理テーブルサイズ
#define REP_INTERVAL    100 // リピート間隔 150msec

uint8_t keyentry[MAXKEYENTRY];    // リピート管理テーブル
uint8_t repeatWait[MAXKEYENTRY];  // リピート開始待ち管理テーブル

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
        sendKeyMake(key);
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
  Serial.print(F("DN ["));  Serial.print(F("mod="));  Serial.print(mod, DEC);
  Serial.print(F(" key="));  Serial.print(key, DEC);  Serial.println(F("]"));
#endif

  //special
  // 変換キーをSHIFTにする
  if (key == 138) {
    Keyboard.press(KEY_LEFT_SHIFT);
    return;
  }
  // 無変換キーをCTRLにする
  if (key == 139) {
    Keyboard.press(KEY_LEFT_CTRL);
    return;
  }
  // 全角半角きりかえ
  if (key == 136) {
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press('`');
    return;
  }

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
  Serial.print(F("UP ["));  Serial.print(F("mod="));  Serial.print(mod, DEC);
  Serial.print(F(" key="));  Serial.print(key, DEC);  Serial.println(F("]"));
#endif

  //special
  // 変換キーをSHIFTにする
  if (key == 138) {
    Keyboard.release(KEY_LEFT_SHIFT);
    return;
  }
  // 無変換キーをCTRLにする
  if (key == 139) {
    Keyboard.release(KEY_LEFT_CTRL);
    return;
  }
  // 全角半角きりかえ
  if (key == 136) {
    Keyboard.release('`');
    Keyboard.release(KEY_LEFT_ALT);
    return;
  }

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

#if MYDEBUG==1
  Serial.print(F("ControlKey ["));  Serial.print(F("before="));  Serial.print(before, DEC);
  Serial.print(F(" after="));  Serial.print(after, DEC);  Serial.println(F("]"));
#endif

  MODIFIERKEYS beforeMod;
  *((uint8_t*)&beforeMod) = before;

  MODIFIERKEYS afterMod;
  *((uint8_t*)&afterMod) = after;

  // 左Ctrlキー
  if (beforeMod.bmLeftCtrl != afterMod.bmLeftCtrl) {
    if (afterMod.bmLeftCtrl) {
      Keyboard.press(KEY_LEFT_CTRL);
    } else {
      Keyboard.release(KEY_LEFT_CTRL);
    }
  }

  // 左Shiftキー
  if (beforeMod.bmLeftShift != afterMod.bmLeftShift) {
    if (afterMod.bmLeftShift) {
      Keyboard.press(KEY_LEFT_SHIFT);
    } else {
      Keyboard.release(KEY_LEFT_SHIFT);
    }
  }

  // 左Altキー
  if (beforeMod.bmLeftAlt != afterMod.bmLeftAlt) {
    if (afterMod.bmLeftAlt) {
      Keyboard.press(KEY_LEFT_ALT);
    } else {
      Keyboard.release(KEY_LEFT_ALT);
    }
  }

  // 左GUIキー(Winキー)
  if (beforeMod.bmLeftGUI != afterMod.bmLeftGUI) {
    if (afterMod.bmLeftGUI) {
      Keyboard.press(KEY_LEFT_GUI);
    } else {
      Keyboard.release(KEY_LEFT_GUI);
    }
  }

  // 右Ctrlキー
  if (beforeMod.bmRightCtrl != afterMod.bmRightCtrl) {
    if (afterMod.bmRightCtrl) {
      Keyboard.press(KEY_RIGHT_CTRL);
    } else {
      Keyboard.release(KEY_RIGHT_CTRL);
    }
  }

  // 右Shiftキー
  if (beforeMod.bmRightShift != afterMod.bmRightShift) {
    if (afterMod.bmRightShift) {
      Keyboard.press(KEY_RIGHT_SHIFT);
    } else {
      Keyboard.release(KEY_RIGHT_SHIFT);
    }
  }

  // 右Altキー
  if (beforeMod.bmRightAlt != afterMod.bmRightAlt) {
    if (afterMod.bmRightAlt) {
      Keyboard.press(KEY_RIGHT_ALT);
    } else {
      Keyboard.release(KEY_RIGHT_ALT);
    };
  }

  // 右GUIキー
  if (beforeMod.bmRightGUI != afterMod.bmRightGUI) {
    if (afterMod.bmRightGUI) {
      Keyboard.press(KEY_RIGHT_GUI);
    } else {
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


