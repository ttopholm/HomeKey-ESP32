#pragma once
// Optional lock status screen for the M5Stack AtomS3R.
//
// Shows the lock state (from LOCK_STATE_CHANGED) as a padlock icon with a
// label, or a neutral "tap your phone" screen that does not reveal it, and
// briefly confirms HomeKey and NFC tag taps. After a period of inactivity the
// screen dims or turns off; taps, the button under the screen and MQTT
// commands (HW_DISPLAY_COMMAND) wake it. Behaviour and texts come from the
// actions config and apply live. Enabled with CONFIG_HK_STATUS_DISPLAY_ATOMS3R.
#include <array>
#include <cstdint>
#include <string>
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
#include "StatusDisplayGfx.hpp"

class ConfigManager;

class StatusDisplay {
public:
  StatusDisplay() = default;
  ~StatusDisplay();
  StatusDisplay(const StatusDisplay&) = delete;
  StatusDisplay& operator=(const StatusDisplay&) = delete;

  /// Claims the display pins, brings up panel and backlight, subscribes to
  /// lock and NFC events and starts the render task. Returns false (and logs
  /// why) if the display could not be started; the rest of the firmware is
  /// unaffected in that case. Labels are read from the actions config and
  /// reloaded whenever they are saved in the web UI.
  bool begin(uint8_t initialLockState, const ConfigManager& config);

private:
  enum class MsgType : uint8_t { LockState, Feedback, Settings, Command };
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
  struct Settings {
    uint8_t mode = 0;       // 0 = lock state, 1 = neutral
    bool idleOff = false;   // off instead of dimmed after the timeout
    uint16_t timeoutS = 30; // 0 = never
    uint8_t rotation = 0;   // quarter turns clockwise
  };
  static constexpr size_t kLabelCount = 10;
  using LabelStore = std::array<std::string, kLabelCount>;

  void loadSettings();
  Settings currentSettings();
  statusgfx::Labels currentLabels(LabelStore& storage);
  void announcePower(bool on);
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
  uint8_t m_rotation = 0;  // applied to every frame in flush(); render-task only after begin()
  SemaphoreHandle_t m_txDone = nullptr;
  QueueHandle_t m_queue = nullptr;
  TaskHandle_t m_task = nullptr;
  uint8_t m_initialState = 3;
  const ConfigManager* m_config = nullptr;
  SemaphoreHandle_t m_settingsMutex = nullptr;
  LabelStore m_customLabels;  // empty entries fall back to defaults
  Settings m_settings;
  AppEventLoop::SubscriptionHandle m_lockEvent;
  AppEventLoop::SubscriptionHandle m_configEvent;
  AppEventLoop::SubscriptionHandle m_nfcEvent;
  AppEventLoop::SubscriptionHandle m_commandEvent;
};

#endif  // CONFIG_HK_STATUS_DISPLAY_ATOMS3R
