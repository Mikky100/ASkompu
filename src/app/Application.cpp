#include "Application.h"

#include "BoardConfig.h"

namespace app {

void Application::setup() {
  storage_.begin();
  storage_.load(settings_);

  display_.begin();
  buttons_.begin();
  speedInput_.begin(BoardConfig::PIN_SPEED_INPUT, BoardConfig::SPEED_INPUT_RISING_EDGE);

  static ui::Layout layout(display_.tft());
  static ui::Renderer renderer(layout);
  layout_ = &layout;
  renderer_ = &renderer;

  clockEditHours_ = settings_.clock.hours();
  clockEditMinutes_ = settings_.clock.minutes();

  lastDiagMs_ = millis();
  lastLoopHzMs_ = millis();
}

void Application::handleEvent(const hal::ButtonEvent& event) {
  if (event.type == hal::ButtonEventType::None) return;

  if (event.id == hal::ButtonId::TripReset && event.type == hal::ButtonEventType::Pressed) {
    trip_.setTotalPulsesAtReset(speedInput_.pulseCount());
    return;
  }

  bool nav = (event.type == hal::ButtonEventType::Pressed || event.type == hal::ButtonEventType::Repeat);
  if (!nav) return;

  switch (menu_.current) {
    case domain::Screen::Main:
    case domain::Screen::Display:
      if (event.id == hal::ButtonId::Left) menu_.current = domain::Screen::Menu;
      break;
    case domain::Screen::Menu:
      if (event.id == hal::ButtonId::Up && menu_.menuIndex > 0) menu_.menuIndex--;
      if (event.id == hal::ButtonId::Down && menu_.menuIndex < 2) menu_.menuIndex++;
      if (event.id == hal::ButtonId::Left) menu_.current = domain::Screen::Main;
      if (event.id == hal::ButtonId::Right) {
        if (menu_.menuIndex == 0) menu_.current = domain::Screen::Display;
        if (menu_.menuIndex == 1) menu_.current = domain::Screen::Settings;
        if (menu_.menuIndex == 2) menu_.current = domain::Screen::Debug;
      }
      break;
    case domain::Screen::Settings:
      if (event.id == hal::ButtonId::Up && menu_.settingsIndex > 0) menu_.settingsIndex--;
      if (event.id == hal::ButtonId::Down && menu_.settingsIndex < 2) menu_.settingsIndex++;
      if (event.id == hal::ButtonId::Left) menu_.current = domain::Screen::Menu;
      if (event.id == hal::ButtonId::Right) {
        if (menu_.settingsIndex == 0) {
          clockEditHours_ = settings_.clock.hours();
          clockEditMinutes_ = settings_.clock.minutes();
          editMinutes_ = false;
          menu_.current = domain::Screen::SettingsClock;
        }
        if (menu_.settingsIndex == 1) menu_.current = domain::Screen::SettingsCoefficient;
        if (menu_.settingsIndex == 2) menu_.current = domain::Screen::SettingsTheme;
      }
      break;
    case domain::Screen::SettingsClock:
      if (event.id == hal::ButtonId::Left) {
        if (editMinutes_) {
          editMinutes_ = false;
        } else {
          menu_.current = domain::Screen::Settings;
        }
      }
      if (event.id == hal::ButtonId::Right) {
        if (!editMinutes_) {
          editMinutes_ = true;
        } else {
          settings_.clock.set(clockEditHours_, clockEditMinutes_);
          storage_.saveClock(clockEditHours_, clockEditMinutes_);
          menu_.current = domain::Screen::Settings;
          editMinutes_ = false;
        }
      }
      if (event.id == hal::ButtonId::Up) {
        if (editMinutes_)
          clockEditMinutes_ = (clockEditMinutes_ + 1) % 60;
        else
          clockEditHours_ = (clockEditHours_ + 1) % 24;
      }
      if (event.id == hal::ButtonId::Down) {
        if (editMinutes_)
          clockEditMinutes_ = (clockEditMinutes_ + 59) % 60;
        else
          clockEditHours_ = (clockEditHours_ + 23) % 24;
      }
      break;
    case domain::Screen::SettingsCoefficient:
      if (event.id == hal::ButtonId::Left) menu_.current = domain::Screen::Settings;
      if (event.id == hal::ButtonId::Up) settings_.coefficient += 10;
      if (event.id == hal::ButtonId::Down && settings_.coefficient > 10) settings_.coefficient -= 10;
      if (event.id == hal::ButtonId::Right) {
        storage_.saveCoefficient(settings_.coefficient);
        menu_.current = domain::Screen::Settings;
      }
      break;
    case domain::Screen::SettingsTheme:
      if (event.id == hal::ButtonId::Left) menu_.current = domain::Screen::Settings;
      if (event.id == hal::ButtonId::Up || event.id == hal::ButtonId::Down) {
        uint8_t idx = static_cast<uint8_t>(settings_.theme);
        idx = (idx + 1) % 3;
        settings_.theme = static_cast<domain::Theme>(idx);
      }
      if (event.id == hal::ButtonId::Right) {
        storage_.saveTheme(settings_.theme);
        menu_.current = domain::Screen::Settings;
      }
      break;
    case domain::Screen::Debug:
      if (event.id == hal::ButtonId::Left) menu_.current = domain::Screen::Menu;
      break;
  }
}

void Application::updateDiagnostics(unsigned long now) {
  settings_.clock.update();

  uint32_t totalPulses = speedInput_.pulseCount();
  uint32_t effectivePulses = trip_.calculateEffectivePulses(totalPulses);
  trip_.updateFromPulses(effectivePulses, settings_.coefficient);

  if (now - lastDiagMs_ >= 200) {
    uint32_t windowPulses = speedInput_.consumeWindowPulses();
    diag_.pulseCount = totalPulses;
    diag_.pulsesWindow = windowPulses;

    float metersPerPulse = 1.0f / static_cast<float>(settings_.coefficient == 0 ? 1 : settings_.coefficient);
    float metersPerSec = (windowPulses * metersPerPulse) / 0.2f;
    diag_.kmh = metersPerSec * 3.6f;

    lastDiagMs_ = now;
  }

  loopCounter_++;
  if (now - lastLoopHzMs_ >= 1000) {
    diag_.loopHz = loopCounter_;
    loopCounter_ = 0;
    lastLoopHzMs_ = now;
  }
}

void Application::render() {
  uint32_t effectivePulses = trip_.calculateEffectivePulses(speedInput_.pulseCount());
  switch (menu_.current) {
    case domain::Screen::Main:
    case domain::Screen::Display:
      renderer_->drawMain(settings_, trip_, effectivePulses);
      break;
    case domain::Screen::Menu:
      renderer_->drawMenu(menu_.menuIndex);
      break;
    case domain::Screen::Settings:
      renderer_->drawSettingsMenu(menu_.settingsIndex);
      break;
    case domain::Screen::SettingsClock:
      renderer_->drawClockEditor(clockEditHours_, clockEditMinutes_, editMinutes_);
      break;
    case domain::Screen::SettingsCoefficient:
      renderer_->drawCoefficientEditor(settings_.coefficient);
      break;
    case domain::Screen::SettingsTheme:
      renderer_->drawThemeEditor(settings_.theme);
      break;
    case domain::Screen::Debug:
      renderer_->drawDebug(diag_, settings_.coefficient);
      break;
  }
}

void Application::loop() {
  buttons_.update();
  handleEvent(buttons_.popEvent());
  updateDiagnostics(millis());
  render();
  delay(16);
}

}  // namespace app
