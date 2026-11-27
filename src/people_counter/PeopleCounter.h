#pragma once

// Anropas en gång från setup(), efter Serial.begin().
void initPeopleCounter();

// Anropas vid varje varv i loop().
void updatePeopleCounter();

// Returnerar det aktuella antalet personer.
int getPeopleCount();
