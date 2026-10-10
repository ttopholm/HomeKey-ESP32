#pragma once
// Optional lock status screen for the M5Stack AtomS3R.
//
// Shows the lock state (from LOCK_STATE_CHANGED) as a padlock icon with a
// label, and briefly confirms HomeKey and NFC tag taps. The backlight dims
// after a period of inactivity and wakes on any event or a press of the
// button under the screen. Enabled with CONFIG_HK_STATUS_DISPLAY_ATOMS3R.
#include <cstdint>
#include <vector>
#include "sdkconfig.h"

#if CONFIG_HK_STATUS_DISPLAY_ATOMS3R

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "app_event_loop.hpp"
#include "GPIOAllocator.hpp"

class StatusDisplay {
public:
  StatusDisplay() = default;
  ~StatusDisplay();
  StatusDisplay(const StatusDisplay&) = delete;
  StatusDisplay& operator=(const StatusDisplay&) = delete;

  /// Claims the display pins, brings up panel and backlight, subscribes to
  /// lock and NFC events and starts the render task. Returns false (and logs
  /// why) if the display could not be started; the rest of the firmware is
  /// unaffected in that case.
  bool begin(uint8_t initialLockState);

private:
  enum class MsgType : uint8_t { LockState, Feedback };
  struct Msg {
    MsgType type;
    uint8_t a;
    uint8_t b;
  };
  enum class Panel : uint8_t { GC9107, ST7735S };

  bool claimPins();
  Panel probePanel();
  bool initPanel(Panel panel);
  bool initBacklight();
  void setBrightness(uint8_t level);
  void flush();
  void post(const Msg& m);
  void subscribeEvents();
  static void taskEntry(void* arg);
  void run();

  std::vector<GPIOAllocator::GPIOLease> m_pins;
  esp_lcd_panel_io_handle_t m_io = nullptr;
  i2c_master_bus_handle_t m_i2cBus = nullptr;
  i2c_master_dev_handle_t m_backlight = nullptr;
  bool m_spiBusOwned = false;
  uint16_t* m_fb = nullptr;
  uint16_t m_colStart = 0;
  uint16_t m_rowStart = 0;
  SemaphoreHandle_t m_txDone = nullptr;
  QueueHandle_t m_queue = nullptr;
  TaskHandle_t m_task = nullptr;
  uint8_t m_initialState = 3;
  AppEventLoop::SubscriptionHandle m_lockEvent;
  AppEventLoop::SubscriptionHandle m_nfcEvent;
};

#endif  // CONFIG_HK_STATUS_DISPLAY_ATOMS3R
