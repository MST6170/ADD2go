// Minimal-Sketch: TFT init, Boot-Screen, 2 Test-Buttons mit Touch
// Isolations-Env falls Schritt-3-Refactor ein Render-Issue einhandelt.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(15, 14);

static void drawBtn(int x, int y, int w, int h, const char* label, uint16_t color) {
    tft.fillRoundRect(x, y, w, h, 6, color);
    tft.setTextColor(TFT_WHITE, color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.drawString(label, x + w / 2, y + h / 2);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[display_test] init");

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.drawString("display_test", 160, 40);

    drawBtn(20, 100, 130, 60, "Btn 1", TFT_ORANGE);
    drawBtn(170, 100, 130, 60, "Btn 2", TFT_BLUE);

    ts.begin();
    ts.setRotation(1);
    Serial.println("[display_test] ready");
}

void loop() {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        Serial.printf("[touch] raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
        delay(150);
    }
}
