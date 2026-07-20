#include <SPI.h>
#include <TFT_eSPI.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();

#define TFT_BL 22

// ── Palette ───────────────────────────────────────────────────────────────────
#define COL_BG      tft.color565(10,  10,  20)
#define COL_TEXT    tft.color565(220, 220, 255)
#define COL_SUBTEXT tft.color565(120, 120, 160)
#define COL_GREEN   tft.color565(0,   230, 120)
#define COL_YELLOW  tft.color565(255, 210, 0  )
#define COL_RED     tft.color565(255, 60,  60 )
#define COL_BLUE    tft.color565(60,  140, 255)
#define COL_SPARK   tft.color565(255, 255, 100)

// ── Battery bar geometry ──────────────────────────────────────────────────────
#define BAR_X   60
#define BAR_Y  105
#define BAR_W  340
#define BAR_H   70
#define BAR_R    8
#define NUB_W   14
#define NUB_H   30

// ── State ─────────────────────────────────────────────────────────────────────
int  lastPercent  = -1;
bool lastCharging = false;
int  animStep     = 0;
unsigned long lastAnim = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────
uint16_t batteryColor(int pct) {
  if (pct > 50) return COL_GREEN;
  if (pct > 20) return COL_YELLOW;
  return COL_RED;
}

void drawBatteryShell(uint16_t col) {
  tft.drawRoundRect(BAR_X - 2, BAR_Y - 2, BAR_W + 4, BAR_H + 4, BAR_R + 2,
                    tft.color565(40, 40, 80));
  tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_R, col);
  tft.fillRoundRect(BAR_X + BAR_W, BAR_Y + (BAR_H - NUB_H) / 2,
                    NUB_W, NUB_H, 4, col);
}

void drawSegmentLines() {
  for (int seg = 1; seg < 4; seg++) {
    int sx = BAR_X + 4 + ((BAR_W - 8) * seg) / 4;
    tft.drawFastVLine(sx, BAR_Y + 4, BAR_H - 8, COL_BG);
  }
}

// ── Fill sweep animation ───────────────────────────────────────────────────────
// Sweeps the bar from left to right in the new color, then settles at actual %
void fillSweepAnimation(int pct, uint16_t newCol, uint16_t oldCol) {
  int innerW = BAR_W - 8;
  int targetW = (innerW * pct) / 100;

  // Phase 1: sweep all the way across to 100%
  for (int w = 0; w <= innerW; w += 6) {
    tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, w, BAR_H - 8, BAR_R - 2, newCol);
    drawSegmentLines();
    delay(4);
  }

  // Brief pause at full
  delay(80);

  // Phase 2: retract back to actual percentage
  for (int w = innerW; w >= targetW; w -= 6) {
    // Fill current width with new color
    tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, innerW, BAR_H - 8, BAR_R - 2, COL_BG);
    tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, w,      BAR_H - 8, BAR_R - 2, newCol);
    drawSegmentLines();
    delay(4);
  }

  // Settle at actual percentage
  tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, innerW, BAR_H - 8, BAR_R - 2, COL_BG);
  if (targetW > 0) {
    tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, targetW, BAR_H - 8, BAR_R - 2, newCol);
  }
  drawSegmentLines();
}

// ── Full screen draw ──────────────────────────────────────────────────────────
void drawScreen(int pct, bool charging, bool animate, uint16_t oldCol) {
  uint16_t barCol = charging ? COL_BLUE : batteryColor(pct);

  tft.fillScreen(COL_BG);

  // Title
  tft.setTextColor(COL_SUBTEXT, COL_BG);
  tft.setTextSize(2);
  tft.setCursor(195, 28);
  tft.print("POWER");

  // Shell
  drawBatteryShell(barCol);

  if (animate) {
    // Sweep animation instead of instant fill
    fillSweepAnimation(pct, barCol, oldCol);
  } else {
    // Instant fill
    int fillW = ((BAR_W - 8) * pct) / 100;
    if (fillW > 0) {
      tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, fillW, BAR_H - 8, BAR_R - 2, barCol);
    }
    drawSegmentLines();
  }

  // Percentage number
  tft.setTextSize(5);
  tft.setTextColor(COL_TEXT, COL_BG);
  String pctStr = String(pct) + "%";
  int textW = pctStr.length() * 30;
  tft.setCursor((480 - textW) / 2, BAR_Y + BAR_H + 20);
  tft.print(pctStr);

  // Status label
  tft.setTextSize(2);
  if (charging) {
    tft.setTextColor(COL_BLUE, COL_BG);
    tft.setCursor(148, BAR_Y + BAR_H + 76);
    tft.print("  ~ CHARGING ~  ");
  } else {
    tft.setTextColor(COL_SUBTEXT, COL_BG);
    tft.setCursor(158, BAR_Y + BAR_H + 76);
    tft.print("ON BATTERY");
  }

  // Low battery warning
  if (pct <= 20 && !charging) {
    tft.setTextColor(COL_RED, COL_BG);
    tft.setTextSize(2);
    tft.setCursor(148, 68);
    tft.print("! LOW BATTERY !");
  }
}

// ── Charging pulse (partial redraw only) ──────────────────────────────────────
void pulseChargingBar(int pct) {
  uint16_t col = (animStep % 2 == 0) ? COL_BLUE
                                      : tft.color565(30, 80, 180);
  int fillW = ((BAR_W - 8) * pct) / 100;
  tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, BAR_W - 8, BAR_H - 8, BAR_R - 2, COL_BG);
  if (fillW > 0) {
    tft.fillRoundRect(BAR_X + 4, BAR_Y + 4, fillW, BAR_H - 8, BAR_R - 2, col);
  }
  drawSegmentLines();
  tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_R, col);
  tft.fillRoundRect(BAR_X + BAR_W, BAR_Y + (BAR_H - NUB_H) / 2,
                    NUB_W, NUB_H, 4, col);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  SPI.setSCK(18);
  SPI.setTX(19);
  SPI.begin();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COL_BG);

  tft.setTextColor(COL_SUBTEXT, COL_BG);
  tft.setTextSize(2);
  tft.setCursor(118, 148);
  tft.print("Connecting...");

  Serial.println("PICO_READY");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("BAT:")) {
      int c1      = line.indexOf(':');
      int c2      = line.indexOf(':', c1 + 1);
      int pct     = line.substring(c1 + 1, c2).toInt();
      bool charging = (line.substring(c2 + 1) == "charging");

      bool stateChanged = (charging != lastCharging);
      bool pctChanged   = (pct != lastPercent);

      uint16_t oldCol = lastCharging ? COL_BLUE : batteryColor(lastPercent);

      lastPercent  = pct;
      lastCharging = charging;

      // Animate sweep when charge state changes, instant update otherwise
      if (stateChanged || lastPercent == -1) {
        drawScreen(pct, charging, true, oldCol);
      } else if (pctChanged) {
        drawScreen(pct, charging, false, oldCol);
      }
    }
  }

  // Pulse bar while charging
  if (lastCharging && millis() - lastAnim > 500) {
    animStep++;
    pulseChargingBar(lastPercent);
    lastAnim = millis();
  }
}
