// jv_reboot.cpp - daily reboot based on NTP / ezTime

#if defined(__has_include)
  #if __has_include("jv_config.h")
    #include "jv_config.h"
  #else
    #include "jv_config_defaults.h"
  #endif
#else
  #include "jv_config_defaults.h"
#endif

#include "jvlib.h"
#include "jv_internal.h"

namespace {
  bool dailyRebootEnabled = false;
  int  lastRebootDay      = -1;   // day-of-year already handled
}

void jvDailyRebootSetup() {
  dailyRebootEnabled = true;
  lastRebootDay = -1;

  // If time is already valid and we're past today's reboot hour,
  // mark today done so a mid-day boot doesn't reboot immediately.
  if (timeStatus() == timeSet) {
    int h = jv_internal::myTZ.hour();
    if (h > JV_REBOOT_HOUR) {
      lastRebootDay = jv_internal::myTZ.dayOfYear();
    }
  }

  LOG_INFO("Daily reboot enabled at %02d:00 local time", JV_REBOOT_HOUR);
}

void jvCheckDailyReboot() {
  if (!dailyRebootEnabled) return;
  if (timeStatus() != timeSet) return;

  int currentHour = jv_internal::myTZ.hour();
  int currentDay  = jv_internal::myTZ.dayOfYear();

  if (currentDay == lastRebootDay) return;

  // Past today's window → mark done, do not reboot
  if (currentHour > JV_REBOOT_HOUR) {
    lastRebootDay = currentDay;
    return;
  }

  // In the target hour (03:00–03:59) → reboot once
  if (currentHour == JV_REBOOT_HOUR) {
    lastRebootDay = currentDay;
    LOG_INFO("Daily reboot triggered (hour=%d dayOfYear=%d)", currentHour, currentDay);
    delay(300);
    ESP.restart();
  }
}
