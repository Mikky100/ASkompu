#include <Arduino.h>

#include "app/Application.h"

app::Application appInstance;

void setup() { appInstance.setup(); }

void loop() { appInstance.loop(); }
