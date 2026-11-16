#include <Servo.h>

// ==========================================
// ピン設定
// ==========================================
const int SERVO_PIN  = 9;   // サーボの信号線（橙/黄）
const int BUTTON_PIN = 2;   // 連打ON/OFF切り替え用タクトスイッチ（片足をD2、もう片足をGNDへ）
const int LED_PIN    = 13;  // 基板上のLED（連打モードONのときに点灯）

// ==========================================
// 角度・タイミング設定（環境に合わせて調整してください）
// ==========================================
int releaseAngle = 90;      // 待機時の角度（ボタンから少し浮いている状態: 0〜180）
int pressAngle   = 105;     // 押下時の角度（ボタンを押す角度: releaseAngle ± 10〜20度程度が目安）

unsigned long holdTimeMs    = 100;  // ボタンを押している時間 (ミリ秒)
unsigned long intervalMinMs = 1000; // 次のクリックまでの最小待機時間 (ミリ秒)
unsigned long intervalMaxMs = 1500; // 次のクリックまでの最大待機時間 (ミリ秒) ※一定間隔にしたい場合はMinと同じ値にする

// true にすると電源ON直後から自動連打を開始します（スイッチを繋がない場合は true にしてください）
bool autoRun = false;

// ==========================================
// 内部状態管理用
// ==========================================
Servo clickerServo;

enum ClickState {
  STATE_IDLE,
  STATE_PRESSING,
  STATE_WAITING
};

ClickState currentState = STATE_IDLE;
unsigned long stateTimer = 0;
unsigned long currentWaitInterval = 1000;

bool lastButtonReading = HIGH;
bool buttonStableState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// 次の待機時間を計算（最小〜最大の間でランダム）
unsigned long getNextInterval() {
  if (intervalMaxMs <= intervalMinMs) {
    return intervalMinMs;
  }
  return random(intervalMinMs, intervalMaxMs + 1);
}

// 単発クリックの開始
void triggerClick() {
  clickerServo.write(pressAngle);
  stateTimer = millis();
  currentState = STATE_PRESSING;
}

void printHelp() {
  Serial.println(F("=========================================="));
  Serial.println(F(" DualSense Auto Clicker Ready!"));
  Serial.println(F(" [シリアルコマンド一覧 (送信すると即反映)]"));
  Serial.println(F("  t        : 単発テストクリック (1回だけ押して戻す)"));
  Serial.println(F("  s        : 自動連打の 開始 / 停止 切り替え"));
  Serial.println(F("  r<角度>  : 待機角度の変更 (例: r90)"));
  Serial.println(F("  p<角度>  : 押下角度の変更 (例: p105)"));
  Serial.println(F("  h<時間>  : 押しっぱなし時間の変更[ms] (例: h120)"));
  Serial.println(F("  i<時間>  : 最小間隔の変更[ms] (例: i1000)"));
  Serial.println(F("  m<時間>  : 最大間隔の変更[ms] (例: m1500)"));
  Serial.println(F("=========================================="));
  printStatus();
}

void printStatus() {
  Serial.print(F("[現在の設定] 連打: "));
  Serial.print(autoRun ? F("ON") : F("OFF"));
  Serial.print(F(" | 待機角度(r): "));
  Serial.print(releaseAngle);
  Serial.print(F("度 | 押下角度(p): "));
  Serial.print(pressAngle);
  Serial.print(F("度 | 押下時間(h): "));
  Serial.print(holdTimeMs);
  Serial.print(F("ms | 間隔(i〜m): "));
  Serial.print(intervalMinMs);
  Serial.print(F("〜"));
  Serial.print(intervalMaxMs);
  Serial.println(F("ms"));
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  // 未接続のアナログピンから乱数シードを初期化
  randomSeed(analogRead(A0));

  // サーボを初期位置（待機角度）にセットしてからアタッチ
  clickerServo.write(releaseAngle);
  clickerServo.attach(SERVO_PIN);

  digitalWrite(LED_PIN, autoRun ? HIGH : LOW);
  if (autoRun) {
    currentWaitInterval = getNextInterval();
    stateTimer = millis();
    currentState = STATE_WAITING;
  }

  printHelp();
}

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonStableState) {
      buttonStableState = reading;
      // ボタンが押された瞬間（LOW）に連打ON/OFFをトグル
      if (buttonStableState == LOW) {
        autoRun = !autoRun;
        digitalWrite(LED_PIN, autoRun ? HIGH : LOW);
        if (autoRun) {
          Serial.println(F(">> 自動連打: 開始"));
          triggerClick(); // 押した瞬間に1発目をクリック
        } else {
          Serial.println(F(">> 自動連打: 停止"));
          clickerServo.write(releaseAngle);
          currentState = STATE_IDLE;
        }
      }
    }
  }
  lastButtonReading = reading;
}

void handleSerial() {
  if (!Serial.available()) return;

  String input = Serial.readStringUntil('\n');
  input.trim();
  if (input.length() == 0) return;

  char cmd = input.charAt(0);
  long val = input.substring(1).toInt();

  switch (cmd) {
    case 't':
    case 'T':
      Serial.println(F(">> 単発テストクリック実行"));
      triggerClick();
      break;

    case 's':
    case 'S':
      autoRun = !autoRun;
      digitalWrite(LED_PIN, autoRun ? HIGH : LOW);
      if (autoRun) {
        Serial.println(F(">> 自動連打: 開始"));
        triggerClick();
      } else {
        Serial.println(F(">> 自動連打: 停止"));
        clickerServo.write(releaseAngle);
        currentState = STATE_IDLE;
      }
      break;

    case 'r':
    case 'R':
      releaseAngle = constrain(val, 0, 180);
      clickerServo.write(releaseAngle);
      printStatus();
      break;

    case 'p':
    case 'P':
      pressAngle = constrain(val, 0, 180);
      printStatus();
      break;

    case 'h':
    case 'H':
      holdTimeMs = max(20L, val);
      printStatus();
      break;

    case 'i':
    case 'I':
      intervalMinMs = max(50L, val);
      if (intervalMaxMs < intervalMinMs) intervalMaxMs = intervalMinMs;
      printStatus();
      break;

    case 'm':
    case 'M':
      intervalMaxMs = max((long)intervalMinMs, val);
      printStatus();
      break;

    default:
      printHelp();
      break;
  }
}

void updateClicker() {
  unsigned long now = millis();

  switch (currentState) {
    case STATE_IDLE:
      if (autoRun) {
        triggerClick();
      }
      break;

    case STATE_PRESSING:
      // 押下時間が経過したらサーボを待機角度に戻す
      if (now - stateTimer >= holdTimeMs) {
        clickerServo.write(releaseAngle);
        stateTimer = now;
        if (autoRun) {
          currentWaitInterval = getNextInterval();
          currentState = STATE_WAITING;
        } else {
          currentState = STATE_IDLE;
        }
      }
      break;

    case STATE_WAITING:
      if (!autoRun) {
        currentState = STATE_IDLE;
      } else if (now - stateTimer >= currentWaitInterval) {
        triggerClick();
      }
      break;
  }
}

void loop() {
  handleButton();
  handleSerial();
  updateClicker();
}
