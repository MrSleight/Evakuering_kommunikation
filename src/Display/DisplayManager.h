
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>



// main klass som funktion ger fakevärde för nod ooch riktining
void displayInitialize();

// Startar OLED-displayen
void beginDisplay();

// Visar riktning
// false = vänster
// true = höger
void displaySetDirection(bool directionRight);

// Visar nodnummer 1, 2 eller 3
void displaySetNodeNumber(uint8_t nodeNumber);

#endif
