#include "storage.h"
#include "config.h"
#include <Preferences.h>

static Preferences prefs;
static bool s_initialized = false;

namespace storage {

void begin() {
    if (s_initialized) return;
    prefs.begin("schaufel", false);
    s_initialized = true;
}

float getTara()              { return prefs.getFloat("tara", 0.0f); }
void  setTara(float v)       { prefs.putFloat("tara", v); }

float getFaktor()            { return prefs.getFloat("faktor", 100.0f); }
void  setFaktor(float v)     { prefs.putFloat("faktor", v); }

int   getFilterAnzahl()      {
    int v = prefs.getInt("filterAnz", FILTER_ANZAHL_DEFAULT);
    if (v < FILTER_ANZAHL_MIN) v = FILTER_ANZAHL_MIN;
    if (v > FILTER_ANZAHL_MAX) v = FILTER_ANZAHL_MAX;
    return v;
}
void  setFilterAnzahl(int v) { prefs.putInt("filterAnz", v); }

int   getFilterSchwelle()    {
    int v = prefs.getInt("filterSchw", FILTER_SCHWELLE_DEFAULT);
    if (v < FILTER_SCHWELLE_MIN) v = FILTER_SCHWELLE_MIN;
    if (v > FILTER_SCHWELLE_MAX) v = FILTER_SCHWELLE_MAX;
    return v;
}
void  setFilterSchwelle(int v){ prefs.putInt("filterSchw", v); }

} // namespace storage
