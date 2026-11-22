/* STI2D-OS userland — the host of the STI2D-OS shell.
 *
 * The STI2D-OS shell (the STI2D-Play launcher and the STI2D-OS V2 layer, in C:
 * firmware/sti2d-play-source-v1/launcher and firmware/v2) is compiled into
 * this Epsilon userland with STI_USERLAND=1. It replaces Epsilon's home screen:
 * every time Epsilon would show its grid of apps, AppsContainer calls
 * RunShell() instead (apps_container.cpp).
 *
 * While the shell runs:
 *  - it gets the external apps RAM as its arena (the RAM an installed app would
 *    get): the current app is torn down and Poincare's pool and tree stack are
 *    deinitialized first, exactly as for an external app, then the RAM is
 *    zeroed before they are rebuilt (their "initialized" flags live there);
 *  - Home is held back (circuit breaker locked): the shell reads Home as a key;
 *  - the EADK functions the shell is written against are implemented here on
 *    top of Ion, and the keyboard scan also watches what Epsilon's run loop
 *    would have handled: On/Off (suspend, then the PIN lock screen), the USB
 *    cable (the calculator becomes reachable by the computer), an empty
 *    battery, and inactivity (dimming, then sleep).
 *
 * The shell returns what the user opened: an Epsilon app (by its place in the
 * home grid), an installed app (its entry point), or Epsilon's own grid. */

#include <apps/apps_container.h>
#include <apps/exam_mode_manager.h>
#include <apps/global_preferences.h>
#include <apps/home/apps_layout.h>
#include <apps/i18n.h>
#include <ion.h>
#include <ion/circuit_breaker.h>
#include <ion/display.h>
#include <ion/led.h>
#include <poincare/src/memory/tree_stack_checkpoint.h>
#include <poincare/pool.h>
#include <setjmp.h>
#include <string.h>

extern "C" {
#include <eadk.h>

#include "v2/include/sti_userland.h"
}

using namespace Poincare;

namespace Ion { namespace LED {
void setBlinking(uint16_t periodInMilliseconds, float dutyCycle);
void setLock(bool locked);
} }

extern "C" {
extern char _external_apps_RAM_start;
extern char _external_apps_RAM_end;
/* The userland header sits at the start of the userland (userland_shared.ld) */
extern const char USERLAND_ORIGIN;
}

namespace Sti2d {

enum Exit { Normal = 0, Suspended = 1, USBEnumerated = 2, BatteryEmpty = 3, Woke = 4 };

static jmp_buf s_exit;
static bool s_inShell = false;
static bool s_locked = true;     // the PIN screen comes first (boot, wake-up)
static bool s_returning = false;  // keep the home screen's selection
static bool s_showGrid = false;   // the shell asked for Epsilon's grid
static bool s_inDFU = false;      // openDFU() switching apps under us
static bool s_externalRunning = false;
static bool s_usbEnabled = false;
static uint64_t s_lastActivity = 0;
static bool s_dimmed = false;
static uint64_t s_lastEventPoll = 0;

constexpr uint64_t k_dimAfter = 30 * 1000;
constexpr uint64_t k_sleepAfter = 5 * 60 * 1000;  // as SuspendTimer
constexpr uint8_t k_dimBrightness = 0;            // as BacklightDimmingTimer

bool Available() { return !ExamModeManager::ExamMode().isActive(); }

/* True when switching to the home snapshot must run the shell instead of
 * showing Epsilon's grid. */
bool ShouldRunShell() {
  if (s_inDFU || !Available()) {
    return false;
  }
  if (s_showGrid) {
    s_showGrid = false;
    return false;
  }
  return true;
}

bool LockOnWake() {
  if (Available() && sti_ul_pin_enabled()) {
    s_locked = true;
  }
  return s_locked;
}

static void restoreBacklight() {
  Ion::Backlight::setBrightness(SharedGlobalPrefs().brightnessLevel());
  s_dimmed = false;
}

/* Everything Epsilon's run loop would do between two events, checked at each
 * keyboard scan of the shell. Leaves the shell (longjmp) when the userland has
 * to take over. */
static uint64_t watch(uint64_t state) {
  uint64_t now = Ion::Timing::millis();
  /* USB: enable the peripheral when a cable is plugged, as AppsContainer does
   * on USBPlug; once a computer has enumerated it, hand over to Epsilon's
   * "calculator is connected" screen (DFU). */
  bool plugged = Ion::USB::isPlugged();
  if (plugged && !s_usbEnabled) {
    Ion::USB::enable();
    s_usbEnabled = true;
    restoreBacklight();
  } else if (!plugged && s_usbEnabled) {
    Ion::USB::disable();
    Ion::USB::clearEnumerationInterrupt();
    s_usbEnabled = false;
  }
  /* The events the kernel produces for the run loop (the shell reads the
   * keyboard state, not events): the enumeration by a computer, and a wake-up
   * when the kernel suspended on On/Off by itself. Key events are dropped. */
  if (now - s_lastEventPoll >= 60) {
    s_lastEventPoll = now;
    for (int i = 0; i < 16; i++) {
      int32_t timeout = 0;
      Ion::Events::Event e = Ion::Events::getEvent(&timeout);
      if (e == Ion::Events::None || e == Ion::Events::TimerFire) {
        break;
      }
      if (e == Ion::Events::USBEnumeration) {
        if (Ion::USB::isPlugged()) {
          longjmp(s_exit, USBEnumerated);
        }
        Ion::USB::clearEnumerationInterrupt();
      } else if (e == Ion::Events::OnOff) {
        longjmp(s_exit, Woke);
      }
    }
  }
  if (Ion::Battery::level() == Ion::Battery::Charge::EMPTY && !plugged) {
    longjmp(s_exit, BatteryEmpty);
  }
  Ion::Keyboard::State keys(state);
  if (keys.keyDown(Ion::Keyboard::Key::OnOff)) {
    longjmp(s_exit, Suspended);
  }
  if (state) {
    s_lastActivity = now;
    if (s_dimmed) {
      restoreBacklight();
    }
  } else if (!s_dimmed && now - s_lastActivity > k_dimAfter) {
    Ion::Backlight::setBrightness(k_dimBrightness);
    s_dimmed = true;
  } else if (now - s_lastActivity > k_sleepAfter && !plugged) {
    longjmp(s_exit, Suspended);
  }
  return state;
}

/* Runs the shell once. Returns how it ended (Exit). */
static int runOnce(sti_ul_request_t* request) {
  uint8_t* arena = reinterpret_cast<uint8_t*>(&_external_apps_RAM_start);
  uint32_t size = &_external_apps_RAM_end - &_external_apps_RAM_start;
  s_lastActivity = Ion::Timing::millis();
  s_usbEnabled = Ion::USB::isPlugged();
  request->kind = STI_UL_NONE;
  Ion::CircuitBreaker::lock();
  s_inShell = true;
  int exit = setjmp(s_exit);
  if (exit == Normal) {
    bool lock = s_locked;
    sti_ul_run(arena, size, lock, s_returning, request);
    s_locked = false;
    s_returning = true;
  }
  s_inShell = false;
  Ion::CircuitBreaker::unlock();
  /* The pool and the tree stack (and their "initialized" flags) live in this
   * RAM: give it back clean. */
  memset(arena, 0, size);
  restoreBacklight();
  return exit;
}

/* The keys pressed in the shell are still queued for Epsilon's run loop:
 * drop them, so the app opened does not get the OK that opened it. */
static void flushEvents() {
  for (int i = 0; i < 64; i++) {
    while (Ion::Keyboard::scan() != 0) {
      Ion::Timing::msleep(10);
    }
    int32_t timeout = 0;
    Ion::Events::Event e = Ion::Events::getEvent(&timeout);
    if (e == Ion::Events::None || e == Ion::Events::TimerFire) {
      return;
    }
  }
}

}  // namespace Sti2d

/* ------------------------------------------------------------------ host */

/* The Classic demo borrows the physical LED for a short visual effect. It is
 * deliberately independent of Ion::ExamMode: no exam flags, restrictions,
 * persistence, reset or authentication state are changed. */
extern "C" void sti_host_demo_exam_led(bool enabled) {
  static bool active = false;
  if (enabled == active) return;
  if (enabled) {
    Ion::LED::setColor(KDColor::RGB24(0xFF4B20));
    Ion::LED::setBlinking(900, 0.35f);
    Ion::LED::setLock(true);
    active = true;
  } else {
    Ion::LED::setLock(false);
    Ion::LED::setColor(Ion::LED::updateColorWithPlugAndCharge());
    active = false;
  }
}

Escher::App::Snapshot* AppsContainer::runSti2dShell() {
  for (;;) {
    /* Leave the current app and free the external apps RAM, as
     * switchToExternalApp does. After Home interrupted an installed app the
     * pool's flags hold that app's garbage: the RAM is only zeroed then. */
    if (activeApp()) {
      resetShiftAlphaStatus();
    }
    Container::switchToBuiltinApp(nullptr);
    Ion::Events::setSpinner(false);
    Ion::Display::setScreenshotCallback(nullptr);
    if (Sti2d::s_externalRunning) {
      Sti2d::s_externalRunning = false;
    } else {
      Internal::TreeStack::SharedTreeStack.deinit();
      Pool::sharedPool.deinit();
    }

    sti_ul_request_t request;
    int exit = Sti2d::runOnce(&request);

    Internal::TreeStack::SharedTreeStack.init();
    Pool::sharedPool.init();
    Ion::Events::setSpinner(true);
    Ion::Display::setScreenshotCallback(ShowCursor);
    m_backlightDimmingTimer.reset();
    m_suspendTimer.reset();

    switch (exit) {
      case Sti2d::Suspended:
        while (Ion::Keyboard::scan().keyDown(Ion::Keyboard::Key::OnOff)) {
          Ion::Timing::msleep(10);
        }
        Ion::Power::suspend(true);
        Ion::Battery::level(true);
        Sti2d::LockOnWake();
        Sti2d::s_returning = true;
        Sti2d::flushEvents();
        continue;
      case Sti2d::Woke:
        // the kernel already suspended (On/Off seen as an event)
        Ion::Battery::level(true);
        Sti2d::LockOnWake();
        Sti2d::s_returning = true;
        Sti2d::flushEvents();
        continue;
      case Sti2d::USBEnumerated:
        Sti2d::s_inDFU = true;
        if (!ExamModeManager::ExamMode().isActive()) {
          openDFU(true);
        }
        Sti2d::s_inDFU = false;
        Sti2d::flushEvents();
        if (!Sti2d::Available()) {
          // the computer turned the exam mode on: Epsilon's home from now on
          return homeAppSnapshot();
        }
        continue;
      case Sti2d::BatteryEmpty:
        shutdownDueToLowBattery();
        Sti2d::flushEvents();
        continue;
      default:
        break;
    }

    Sti2d::flushEvents();
    if (request.kind == STI_UL_BUILTIN && request.index >= 0 &&
        request.index < sti_host_builtin_count()) {
      return appSnapshotAtIndex(Home::PermutedAppSnapshotIndex(request.index + 1));
    }
    if (request.kind == STI_UL_GRID) {
      Sti2d::s_showGrid = true;
      return homeAppSnapshot();
    }
    if (request.kind == STI_UL_EXTERNAL && request.entry) {
      /* As switchToExternalApp: the app gets the external apps RAM. If Home
       * interrupts it, AppsContainer::run() comes back here through
       * switchToBuiltinApp(home). */
      reloadTitleBarView();
      Ion::Events::setSpinner(false);
      Ion::Display::setScreenshotCallback(nullptr);
      Internal::TreeStack::SharedTreeStack.deinit();
      Pool::sharedPool.deinit();
      Sti2d::s_externalRunning = true;
      request.entry();
      /* Back in the shell: its RAM is the app's leftovers, zeroed by the
       * shell's run (s_externalRunning: nothing to deinitialize). */
      memset(&_external_apps_RAM_start, 0,
             &_external_apps_RAM_end - &_external_apps_RAM_start);
      Sti2d::flushEvents();
      continue;
    }
  }
}

bool AppsContainer::sti2dDidSuspend() { return Sti2d::LockOnWake(); }

/* ------------------------------------------------------------------ shell API */

extern "C" {

const void* sti_host_userland_header(void) { return &USERLAND_ORIGIN; }

int sti_host_builtin_count(void) {
  // the home app (index 0) is not one of them
  return AppsContainer::sharedAppsContainer()->numberOfBuiltinApps() - 1;
}

static const Escher::App::Descriptor* builtinDescriptor(int i) {
  AppsContainer* c = AppsContainer::sharedAppsContainer();
  return c->appSnapshotAtIndex(Home::PermutedAppSnapshotIndex(i + 1))->descriptor();
}

const char* sti_host_builtin_name(int i) {
  if (i < 0 || i >= sti_host_builtin_count()) {
    return "?";
  }
  return I18n::translate(builtinDescriptor(i)->name());
}

bool sti_host_builtin_allowed(int i) {
  if (i < 0 || i >= sti_host_builtin_count()) {
    return false;
  }
  I18n::Message name = builtinDescriptor(i)->name();
  ExamMode examMode = ExamModeManager::ExamMode();
  return !((name == I18n::Message::CodeApp && examMode.forbidCodeApp()) ||
           (name == I18n::Message::ElementsApp && examMode.forbidElementsApp()) ||
           (name == I18n::Message::SolverApp && examMode.forbidSolverApp()));
}

int sti_host_builtin_settings(void) {
  for (int i = 0; i < sti_host_builtin_count(); i++) {
    if (builtinDescriptor(i)->name() == I18n::Message::SettingsApp) {
      return i;
    }
  }
  return -1;
}

/* ---- EADK, on top of Ion. Only the shell calls these: installed apps bring
 * their own EADK (system calls to the kernel). */

extern const char eadk_app_name[] = "STI2D-OS";
extern const uint32_t eadk_api_level = 0;
/* STI2D-Play's start flags (launcher/app_name.c): start as itself, no disguise */
extern const unsigned char np_defaults[4] = {'D', 1, 0, 0};

eadk_keyboard_state_t eadk_keyboard_scan() {
  uint64_t state = Ion::Keyboard::scan();
  return Sti2d::s_inShell ? Sti2d::watch(state) : state;
}

eadk_event_t eadk_event_get(int32_t* timeout) {
  Ion::Events::Event e = Ion::Events::getEvent(timeout);
  return static_cast<eadk_event_t>(static_cast<uint8_t>(e));
}

void eadk_display_push_rect(eadk_rect_t rect, const eadk_color_t* pixels) {
  Ion::Display::pushRect(KDRect(rect.x, rect.y, rect.width, rect.height),
                         reinterpret_cast<const KDColor*>(pixels));
}

void eadk_display_push_rect_uniform(eadk_rect_t rect, eadk_color_t color) {
  Ion::Display::pushRectUniform(KDRect(rect.x, rect.y, rect.width, rect.height),
                                KDColor::RGB16(color));
}

void eadk_display_pull_rect(eadk_rect_t rect, eadk_color_t* pixels) {
  Ion::Display::pullRect(KDRect(rect.x, rect.y, rect.width, rect.height),
                         reinterpret_cast<KDColor*>(pixels));
}

bool eadk_display_wait_for_vblank() { return Ion::Display::waitForVBlank(); }

void eadk_display_draw_string(const char* text, eadk_point_t point,
                              bool large_font, eadk_color_t text_color,
                              eadk_color_t background_color) {
  KDContext* ctx = Ion::Display::Context::SharedContext;
  ctx->setOrigin(KDPointZero);
  ctx->setClippingRect(KDRect(0, 0, Ion::Display::Width, Ion::Display::Height));
  ctx->drawString(text, KDPoint(point.x, point.y),
                  {.glyphColor = KDColor::RGB16(text_color),
                   .backgroundColor = KDColor::RGB16(background_color),
                   .font = large_font ? KDFont::Size::Large : KDFont::Size::Small});
}

void eadk_timing_usleep(uint32_t us) { Ion::Timing::usleep(us); }
void eadk_timing_msleep(uint32_t ms) { Ion::Timing::msleep(ms); }
uint64_t eadk_timing_millis() { return Ion::Timing::millis(); }

bool eadk_usb_is_plugged() { return Ion::USB::isPlugged(); }
uint32_t eadk_random() { return Ion::random(); }

void eadk_backlight_set_brightness(uint8_t brightness) {
  Ion::Backlight::setBrightness(brightness);
}
uint8_t eadk_backlight_brightness() { return Ion::Backlight::brightness(); }
bool eadk_battery_is_charging() { return Ion::Battery::isCharging(); }
uint8_t eadk_battery_level() { return static_cast<uint8_t>(Ion::Battery::level()); }
float eadk_battery_voltage() { return Ion::Battery::voltage(); }

}  // extern "C"
