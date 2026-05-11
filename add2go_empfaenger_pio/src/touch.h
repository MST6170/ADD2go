// Touch-Init + Routing pro Menue
// Migriert aus add2go_empfaenger_v1_0_2.ino Z. 274-278, 621-868

#pragma once

namespace touch {

void begin();      // ts.begin() + setRotation(1)
void handle();     // Liest Touch, dispatcht nach menuState

} // namespace touch
