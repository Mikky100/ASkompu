#include "app/Application.h"

app::Application g_app;

void setup() {
  g_app.setup();
}

void loop() {
  g_app.loop();
}
