// ADS1115 + Reed-Sensor + Filter (Mittelwert + Hysterese) + ADS-Watchdog
// Migriert aus add2go_empfaenger_v1_0_2.ino Z. 195-203, 511-578

#pragma once

namespace scale {

void begin();             // Wire.begin(SDA,SCL) + ads.begin() + setGain
void resetFilter();       // Filter-Buffer komplett zuruecksetzen
void handleReed();        // Reed-Kontakt lesen -> schaufelInPosition
void handleADC();         // 10 Hz ADC-Sample + Filter + ADS-Watchdog

} // namespace scale
