
#pragma once

#include "MessageTypes.h"

// Initialise the messaging system
void beginMessageManager();

// Process incoming and outgoing messages
void updateMessageManager();

// Update sensor buffers
void updateSensorA(bool value);
void updateSensorB(int value);

// Update blockage state
void updateBlockage(int id, bool blocked);
