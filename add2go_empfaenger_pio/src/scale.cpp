#include "scale.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

static Adafruit_ADS1115 ads;

namespace scale {

void begin() {
    pinMode(REED_SENSOR_PIN, INPUT);
    Wire.begin(I2C_SDA, I2C_SCL);
    if (ads.begin()) {
        Serial.println("[OK] ADS1115 gefunden");
        ads.setGain(GAIN_ONE);
        adsVorhanden = true;
    } else {
        Serial.println("[FEHLER] ADS1115 nicht gefunden!");
        adsVorhanden = false;
    }
}

void resetFilter() {
    for (int i = 0; i < FILTER_BUFFER_SIZE; i++) {
        filterBuffer[i] = 0;
    }
    filterBufferIndex = 0;
    filterBufferCount = 0;
    letzteAngezeigtesGewicht = 0;
    schaufelGewichtGefiltert = 0;
}

void handleReed() {
    schaufelInPosition = (digitalRead(REED_SENSOR_PIN) == LOW);
}

void handleADC() {
    if (!adsVorhanden) return;
    if (millis() - lastADCRead < ADC_INTERVAL_MS) return;
    lastADCRead = millis();

    int16_t rawADC = ads.readADC_SingleEnded(0);

    // ADS-Watchdog: Fehlerwerte
    if (rawADC == -1) {
        adsFehlerZaehler++;
        if (adsFehlerZaehler >= ADS_FEHLER_LIMIT) {
            Serial.println("[FEHLER] ADS1115 liefert Fehlerwerte - als ausgefallen markiert");
            adsVorhanden = false;
            adsFehlerZaehler = 0;
        }
        return;
    } else {
        adsFehlerZaehler = 0;
    }

    // Stuck-Detection: identischer Wert
    if (rawADC == adsLetzterRawWert) {
        adsGleicheWerteZaehler++;
        if (adsGleicheWerteZaehler >= ADS_STUCK_LIMIT) {
            Serial.println("[FEHLER] ADS1115 haengt - als ausgefallen markiert");
            adsVorhanden = false;
            adsGleicheWerteZaehler = 0;
            return;
        }
    } else {
        adsGleicheWerteZaehler = 0;
        adsLetzterRawWert = rawADC;
    }

    schaufelRohwert = rawADC * 0.000125f;

    if (schaufelInPosition) {
        float spannungBereinigt = schaufelRohwert - schaufelTara;
        schaufelGewicht = (int)(spannungBereinigt * schaufelFaktor);
        if (schaufelGewicht < 0) schaufelGewicht = 0;

        // In Filter-Buffer speichern
        filterBuffer[filterBufferIndex] = schaufelGewicht;
        filterBufferIndex = (filterBufferIndex + 1) % FILTER_BUFFER_SIZE;
        if (filterBufferCount < FILTER_BUFFER_SIZE) filterBufferCount++;

        // Mittelwert ueber filterAnzahl Werte
        float summe = 0;
        int anzahl = min(filterAnzahl, filterBufferCount);
        for (int i = 0; i < anzahl; i++) {
            int idx = (filterBufferIndex - 1 - i + FILTER_BUFFER_SIZE) % FILTER_BUFFER_SIZE;
            summe += filterBuffer[idx];
        }
        int mittelwert = (int)(summe / anzahl);

        // Hysterese
        if (abs(mittelwert - letzteAngezeigtesGewicht) >= filterSchwelle) {
            schaufelGewichtGefiltert = mittelwert;
            letzteAngezeigtesGewicht = mittelwert;
        }
    }
}

} // namespace scale
