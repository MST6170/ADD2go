// NVS-Wrapper (Preferences) - Namespace "schaufel"
// Migriert aus add2go_empfaenger_v1_0_2.ino Z. 242-263

#pragma once

#include <Arduino.h>

namespace storage {

void begin();   // Preferences.begin("schaufel", false), idempotent

// Schaufel-Tara (Spannung) und Faktor (kg/V)
float getTara();
void  setTara(float v);
float getFaktor();
void  setFaktor(float v);

// Filter-Konfiguration
int  getFilterAnzahl();
void setFilterAnzahl(int v);
int  getFilterSchwelle();
void setFilterSchwelle(int v);

} // namespace storage
