#include "Application.h"

#include "BoardConfig.h"

namespace app {

domain::Screen Application::nextScreen(domain::Screen current) const {
  switch (current) {
    case domain::Screen::SettingsClock:
      return domain::Screen::SettingsCoefficient;
    case domain::Screen::SettingsCoefficient:
      return domain::Screen::SettingsTheme;
    case domain::Screen::SettingsTheme:
      return domain::Screen::Main;
    case domain::Screen::Main:
      return domain::Screen::Debug;
    case domain::Screen::Debug:
    default:
      return domain::Screen::SettingsClock;
  }
}

domain::Screen Application::prevScreen(domain::Screen current) const {
  switch (current) {
    case domain::Screen::SettingsClock:
      return domain::Screen::Debug;
    case domain::Screen::SettingsCoefficient:
      return domain::Screen::SettingsClock;
    case domain::Screen::SettingsTheme:
      return domain::Screen::SettingsCoefficient;
    case domain::Screen::Main:
      return domain::Screen::SettingsTheme;
    case domain::Screen::Debug:
    default:
      return domain::Screen::Main;
  }
}

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

  menu_.current = domain::Screen::SettingsClock;
  editMinutes_ = false;

  lastDiagMs_ = millis();
  lastLoopHzMs_ = millis();
  lastRenderMs_ = 0;
  renderDirty_ = true;
}

void Application::handleEvent(const hal::ButtonEvent& event) {
  if (event.type == hal::ButtonEventType::None) return;

  if (event.id == hal::ButtonId::TripReset && event.type == hal::ButtonEventType::Pressed) {
    trip_.setTotalPulsesAtReset(speedInput_.pulseCount());
    renderDirty_ = true;
    return;
  }

  bool nav = (event.type == hal::ButtonEventType::Pressed ||
              event.type == hal::ButtonEventType::Repeat);
  if (!nav) return;

  if (menu_.current == domain::Screen::SettingsClock) {
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
    if (event.id == hal::ButtonId::Right) {
      if (!editMinutes_) {
        editMinutes_ = true;
      } else {
        settings_.clock.set(clockEditHours_, clockEditMinutes_);
        editMinutes_ = false;
        menu_.current = nextScreen(menu_.current);
      }
    }
    if (event.id == hal::ButtonId::Left) {
      if (editMinutes_) {
        editMinutes_ = false;
      } else {
        menu_.current = prevScreen(menu_.current);
      }
    }
    renderDirty_ = true;
    return;
  }

  if (event.id == hal::ButtonId::Left) {
    menu_.current = prevScreen(menu_.current);
    renderDirty_ = true;
    return;
  }
  if (event.id == hal::ButtonId::Right) {
    menu_.current = nextScreen(menu_.current);
    renderDirty_ = true;
    return;
  }

  if (menu_.current == domain::Screen::SettingsCoefficient) {
    if (event.id == hal::ButtonId::Up) settings_.coefficient += 10;
    if (event.id == hal::ButtonId::Down && settings_.coefficient > 10) settings_.coefficient -= 10;
    storage_.saveCoefficient(settings_.coefficient);
    renderDirty_ = true;
    return;
  }

  if (menu_.current == domain::Screen::SettingsTheme) {
    if (event.id == hal::ButtonId::Up) {
      uint8_t idx = static_cast<uint8_t>(settings_.theme);
      idx = (idx + 1) % 3;
      settings_.theme = static_cast<domain::Theme>(idx);
      storage_.saveTheme(settings_.theme);
    }
    if (event.id == hal::ButtonId::Down) {
      uint8_t idx = static_cast<uint8_t>(settings_.theme);
      idx = (idx + 2) % 3;
      settings_.theme = static_cast<domain::Theme>(idx);
      storage_.saveTheme(settings_.theme);
    }
    renderDirty_ = true;
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

    float metersPerPulse =
        1000.0f / static_cast<float>(settings_.coefficient == 0 ? 1 : settings_.coefficient);
    float metersPerSec = (windowPulses * metersPerPulse) / 0.2f;
    diag_.kmh = metersPerSec * 3.6f;

    lastDiagMs_ = now;
    if (menu_.current == domain::Screen::Debug || menu_.current == domain::Screen::Main) {
      renderDirty_ = true;
    }
  }

  loopCounter_++;
  if (now - lastLoopHzMs_ >= 1000) {
    diag_.loopHz = loopCounter_;
    loopCounter_ = 0;
    lastLoopHzMs_ = now;
    if (menu_.current == domain::Screen::Debug) renderDirty_ = true;
  }
}

void Application::render() {
  unsigned long now = millis();
  bool periodicMain = (menu_.current == domain::Screen::Main) && (now - lastRenderMs_ >= 250);
  if (!renderDirty_ && !periodicMain) {
    return;
  }

  uint32_t effectivePulses = trip_.calculateEffectivePulses(speedInput_.pulseCount());
  switch (menu_.current) {
    case domain::Screen::Main:
      renderer_->drawMain(settings_, trip_, points_, effectivePulses);
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

  lastRenderMs_ = now;
  renderDirty_ = false;
}

void Application::loop() {
  buttons_.update();
  handleEvent(buttons_.popEvent());
  updateDiagnostics(millis());
  render();
  delay(10);
}

}  // namespace app
