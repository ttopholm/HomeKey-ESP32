#include "StatusDisplay.hpp"

#if CONFIG_HK_STATUS_DISPLAY_ATOMS3R

#include <cstring>
#include <span>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "eventStructs.hpp"
#include "app_events.hpp"
#include "StatusDisplayGfx.hpp"
#include "ConfigManager.hpp"
#include "config.hpp"

namespace {
const char* TAG = "StatusDisplay";

// --- M5Stack AtomS3R wiring (from M5Stack's M5GFX board definition) ---------
constexpr gpio_num_t PIN_LCD_MOSI = GPIO_NUM_21;
constexpr gpio_num_t PIN_LCD_SCLK = GPIO_NUM_15;
constexpr gpio_num_t PIN_LCD_CS = GPIO_NUM_14;
constexpr gpio_num_t PIN_LCD_DC = GPIO_NUM_42;
constexpr gpio_num_t PIN_LCD_RST = GPIO_NUM_48;
// The backlight is driven by an LP5562 LED driver on the internal I2C bus.
constexpr gpio_num_t PIN_SYS_SDA = GPIO_NUM_45;
constexpr gpio_num_t PIN_SYS_SCL = GPIO_NUM_0;
constexpr uint16_t LP5562_ADDR = 0x30;
// Button under the screen, active low.
constexpr gpio_num_t PIN_BUTTON = GPIO_NUM_41;

constexpr spi_host_device_t LCD_HOST = SPI3_HOST;
constexpr int LCD_SIZE = 128;
constexpr int LCD_PCLK_HZ = 40 * 1000 * 1000;
constexpr size_t FB_BYTES = LCD_SIZE * LCD_SIZE * 2;

constexpr uint8_t CMD_DELAY = 0x80;  // length byte flag: a delay byte follows the args
constexpr uint8_t CMD_INVOFF = 0x20, CMD_INVON = 0x21, CMD_CASET = 0x2A, CMD_RASET = 0x2B,
                  CMD_RAMWR = 0x2C, CMD_MADCTL = 0x36, CMD_COLMOD = 0x3A;

// Init sequences, format: cmd, n_args | CMD_DELAY, args..., [delay_ms (255 = 500 ms)].
// Taken from LovyanGFX/M5GFX (Panel_GC9107, Panel_ST7735S).
constexpr uint8_t kInitGC9107[] = {
    0xFE, 0 | CMD_DELAY, 5,
    0xEF, 0 | CMD_DELAY, 5,
    0xB0, 1, 0xC0,
    0xB2, 1, 0x2F,
    0xB3, 1, 0x03,
    0xB6, 1, 0x19,
    0xB7, 1, 0x01,
    0xAC, 1, 0xCB,
    0xAB, 1, 0x0E,
    0xB4, 1, 0x04,
    0xA8, 1, 0x19,
    0xB8, 1, 0x08,
    0xE8, 1, 0x24,
    0xE9, 1, 0x48,
    0xEA, 1, 0x22,
    0xC6, 1, 0x30,
    0xC7, 1, 0x18,
    0xF0, 14, 0x01, 0x2b, 0x23, 0x3c, 0xb7, 0x12, 0x17, 0x60, 0x00, 0x06, 0x0c, 0x17, 0x12, 0x1f,
    0xF1, 14, 0x05, 0x2e, 0x2d, 0x44, 0xd6, 0x15, 0x17, 0xa0, 0x02, 0x0d, 0x0d, 0x1a, 0x18, 0x1f,
    0x11, 0 | CMD_DELAY, 120,
    0x29, 0,
    0xFF, 0xFF,
};

constexpr uint8_t kInitST7735S[] = {
    0x01, 0 | CMD_DELAY, 150,          // SWRESET
    0x11, 0 | CMD_DELAY, 255,          // SLPOUT
    0xB1, 3, 0x01, 0x2C, 0x2D,         // FRMCTR1
    0xB2, 3, 0x01, 0x2C, 0x2D,         // FRMCTR2
    0xB3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,  // FRMCTR3
    0xB4, 1, 0x07,                     // INVCTR
    0xC0, 3, 0xA2, 0x02, 0x84,         // PWCTR1
    0xC1, 1, 0xC5,                     // PWCTR2
    0xC2, 2, 0x0A, 0x00,               // PWCTR3
    0xC3, 2, 0x8A, 0x2A,               // PWCTR4
    0xC4, 2, 0x8A, 0xEE,               // PWCTR5
    0xC5, 1, 0x0E,                     // VMCTR1
    0xE0, 16, 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d,
              0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,  // GMCTRP1
    0xE1, 16, 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
              0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,  // GMCTRN1
    0x13, 0 | CMD_DELAY, 10,           // NORON
    0x29, 0 | CMD_DELAY, 100,          // DISPON
    0xFF, 0xFF,
};

statusgfx::Language language() {
#if CONFIG_HK_STATUS_DISPLAY_LANG_DA
  return statusgfx::Language::Danish;
#else
  return statusgfx::Language::English;
#endif
}

int64_t nowMs() { return esp_timer_get_time() / 1000; }

bool IRAM_ATTR onColorDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &woken);
  return woken == pdTRUE;
}
}  // namespace

StatusDisplay::~StatusDisplay() {
  m_lockEvent.reset();
  m_configEvent.reset();
  m_nfcEvent.reset();
  m_commandEvent.reset();
  if (m_task) vTaskDelete(m_task);
  if (m_io) esp_lcd_panel_io_del(m_io);
  if (m_spiBusOwned) spi_bus_free(LCD_HOST);
  if (m_backlight) i2c_master_bus_rm_device(m_backlight);
  if (m_i2cBus) i2c_del_master_bus(m_i2cBus);
  if (m_fb) heap_caps_free(m_fb);
  if (m_queue) vQueueDelete(m_queue);
  if (m_txDone) vSemaphoreDelete(m_txDone);
  if (m_settingsMutex) vSemaphoreDelete(m_settingsMutex);
}

bool StatusDisplay::claimPins() {
  using A = GPIOAllocator;
  struct Want {
    gpio_num_t pin;
    gpio_mode_t mode;
    A::PinRole role;
    const char* tag;
  };
  const Want wants[] = {
      {PIN_LCD_MOSI, GPIO_MODE_OUTPUT, A::PinRole::SpiMosi, "LCD_MOSI"},
      {PIN_LCD_SCLK, GPIO_MODE_OUTPUT, A::PinRole::SpiSck, "LCD_SCLK"},
      {PIN_LCD_CS, GPIO_MODE_OUTPUT, A::PinRole::SpiCs, "LCD_CS"},
      {PIN_LCD_DC, GPIO_MODE_OUTPUT, A::PinRole::GpioOut, "LCD_DC"},
      {PIN_LCD_RST, GPIO_MODE_OUTPUT, A::PinRole::GpioOut, "LCD_RST"},
      {PIN_SYS_SDA, GPIO_MODE_INPUT_OUTPUT_OD, A::PinRole::I2cSda, "SYS_I2C_SDA"},
      {PIN_SYS_SCL, GPIO_MODE_INPUT_OUTPUT_OD, A::PinRole::I2cScl, "SYS_I2C_SCL"},
      {PIN_BUTTON, GPIO_MODE_INPUT, A::PinRole::GpioIn, "DISPLAY_BUTTON"},
  };
  for (const auto& w : wants) {
    auto lease = A::instance().acquire(w.pin, w.mode, w.role, A::PinConsumer::Display, w.tag);
    if (!lease) {
      ESP_LOGE(TAG, "GPIO %d (%s) %s - status display disabled", w.pin, w.tag,
               A::error_str(lease.error()));
      m_pins.clear();
      return false;
    }
    m_pins.push_back(std::move(*lease));
  }
  m_pins.back().set_pullup(true);  // button
  return true;
}

StatusDisplay::Panel StatusDisplay::probePanel() {
  // AtomS3R units ship with either a GC9107 or an ST7735S. Read the display
  // ID (RDDID, 0x04) over the bidirectional data line to tell them apart.
  gpio_set_level(PIN_LCD_DC, 0);
  for (int clock : {1 * 1000 * 1000, 100 * 1000}) {  // some GC9107s only answer slowly
    spi_device_interface_config_t dev = {};
    dev.command_bits = 8;
    dev.dummy_bits = 1;
    dev.mode = 0;
    dev.clock_speed_hz = clock;
    dev.spics_io_num = PIN_LCD_CS;
    dev.flags = SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX;
    dev.queue_size = 1;
    spi_device_handle_t h = nullptr;
    if (spi_bus_add_device(LCD_HOST, &dev, &h) != ESP_OK) break;
    spi_transaction_t t = {};
    t.cmd = 0x04;
    t.rxlength = 32;
    t.flags = SPI_TRANS_USE_RXDATA;
    esp_err_t err = spi_device_polling_transmit(h, &t);
    spi_bus_remove_device(h);
    if (err != ESP_OK) continue;
    const uint32_t id = t.rx_data[0] | (t.rx_data[1] << 8) | (t.rx_data[2] << 16) |
                        (uint32_t(t.rx_data[3]) << 24);
    ESP_LOGI(TAG, "Panel ID @%d Hz: 0x%08lx", clock, (unsigned long)id);
    if ((id & 0xFFFFFF) == 0x079100) return Panel::GC9107;
    if ((id & 0xFFFF) == 0x7683 || (id & 0xFFFF) == 0x897C) return Panel::ST7735S;
  }
  ESP_LOGW(TAG, "Panel ID not recognised, assuming GC9107");
  return Panel::GC9107;
}

bool StatusDisplay::initPanel(Panel panel) {
  esp_lcd_panel_io_spi_config_t io = {};
  io.cs_gpio_num = PIN_LCD_CS;
  io.dc_gpio_num = PIN_LCD_DC;
  io.spi_mode = 0;
  io.pclk_hz = LCD_PCLK_HZ;
  io.trans_queue_depth = 4;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  io.on_color_trans_done = onColorDone;
  io.user_ctx = m_txDone;
  if (esp_err_t err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io, &m_io);
      err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(err));
    return false;
  }

  const uint8_t* p = panel == Panel::GC9107 ? kInitGC9107 : kInitST7735S;
  for (;;) {
    const uint8_t cmd = *p++;
    uint8_t n = *p++;
    if (cmd == 0xFF && n == 0xFF) break;
    const bool delay = n & CMD_DELAY;
    n &= ~CMD_DELAY;
    esp_lcd_panel_io_tx_param(m_io, cmd, n ? p : nullptr, n);
    p += n;
    if (delay) {
      const uint8_t ms = *p++;
      vTaskDelay(pdMS_TO_TICKS(ms == 255 ? 500 : ms));
    }
  }

  // Orientation, offsets and inversion as M5GFX sets them up for this board,
  // so the picture is upright with the USB port at the bottom.
  uint8_t madctl;
  bool invert;
  if (panel == Panel::GC9107) {
    madctl = 0x08;  // BGR
    invert = false;
    m_colStart = 0;
    m_rowStart = 32;
  } else {
    madctl = 0xC0 | 0x10 | 0x04 | 0x08;  // MY | MX | ML | MH | BGR (180 degrees)
    invert = true;
    m_colStart = 2;
    m_rowStart = 3;
  }
  const uint8_t colmod = 0x55;  // RGB565
  esp_lcd_panel_io_tx_param(m_io, CMD_COLMOD, &colmod, 1);
  esp_lcd_panel_io_tx_param(m_io, CMD_MADCTL, &madctl, 1);
  esp_lcd_panel_io_tx_param(m_io, invert ? CMD_INVON : CMD_INVOFF, nullptr, 0);
  return true;
}

bool StatusDisplay::initBacklight() {
  i2c_master_bus_config_t bus = {};
  bus.i2c_port = -1;  // any free port; the NFC reader may already hold one
  bus.sda_io_num = PIN_SYS_SDA;
  bus.scl_io_num = PIN_SYS_SCL;
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.flags.enable_internal_pullup = true;
  if (esp_err_t err = i2c_new_master_bus(&bus, &m_i2cBus); err != ESP_OK) {
    ESP_LOGE(TAG, "Backlight I2C bus failed: %s", esp_err_to_name(err));
    return false;
  }
  i2c_device_config_t dev = {};
  dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev.device_address = LP5562_ADDR;
  dev.scl_speed_hz = 400000;
  if (esp_err_t err = i2c_master_bus_add_device(m_i2cBus, &dev, &m_backlight); err != ESP_OK) {
    ESP_LOGE(TAG, "Backlight device failed: %s", esp_err_to_name(err));
    return false;
  }
  auto reg = [this](uint8_t r, uint8_t v) {
    const uint8_t buf[2] = {r, v};
    return i2c_master_transmit(m_backlight, buf, sizeof(buf), 50) == ESP_OK;
  };
  bool ok = reg(0x00, 0x40);  // ENABLE: chip enable
  vTaskDelay(pdMS_TO_TICKS(1));
  ok = ok && reg(0x08, 0x01);  // CONFIG: internal clock
  ok = ok && reg(0x70, 0x00);  // LED MAP: all channels under direct I2C control
  if (!ok) ESP_LOGE(TAG, "LP5562 backlight driver did not respond");
  return ok;
}

void StatusDisplay::setBrightness(uint8_t level) {
  if (!m_backlight) return;
  const uint8_t buf[2] = {0x0E, level};  // W channel PWM
  i2c_master_transmit(m_backlight, buf, sizeof(buf), 50);
}

void StatusDisplay::flush() {
  // Every frame is drawn from scratch, so rotating the finished frame in
  // place is enough to support a sideways or upside-down mounted screen.
  statusgfx::rotateSquare(m_fb, LCD_SIZE, m_rotation);
  const uint16_t xs = m_colStart, xe = m_colStart + LCD_SIZE - 1;
  const uint16_t ys = m_rowStart, ye = m_rowStart + LCD_SIZE - 1;
  const uint8_t col[4] = {uint8_t(xs >> 8), uint8_t(xs), uint8_t(xe >> 8), uint8_t(xe)};
  const uint8_t row[4] = {uint8_t(ys >> 8), uint8_t(ys), uint8_t(ye >> 8), uint8_t(ye)};
  esp_lcd_panel_io_tx_param(m_io, CMD_CASET, col, 4);
  esp_lcd_panel_io_tx_param(m_io, CMD_RASET, row, 4);
  esp_lcd_panel_io_tx_color(m_io, CMD_RAMWR, m_fb, FB_BYTES);
}

void StatusDisplay::post(const Msg& m) {
  if (m_queue) xQueueSend(m_queue, &m, 0);
}

void StatusDisplay::subscribeEvents() {
  m_configEvent = AppEventLoop::subscribe(
      HW_EVENT, HW_CONFIG_CHANGED, [this](const uint8_t* data, size_t size) {
        if (size == 0 || data == nullptr) return;
        std::span<const uint8_t> payload(data, size);
        std::error_code ec;
        EventValueChanged ev = alpaca::deserialize<EventValueChanged>(payload, ec);
        if (ec || ev.name != "displaySettings") return;
        loadSettings();
        post({MsgType::Settings, 0, 0});
      });
  m_commandEvent = AppEventLoop::subscribe(
      HW_EVENT, HW_DISPLAY_COMMAND, [this](const uint8_t* data, size_t size) {
        if (size < 1 || data == nullptr) return;
        post({MsgType::Command, data[0], 0});
      });
  m_lockEvent = AppEventLoop::subscribe(
      LOCK_EVENT, LOCK_STATE_CHANGED, [this](const uint8_t* data, size_t size) {
        if (size == 0 || data == nullptr) return;
        std::span<const uint8_t> payload(data, size);
        std::error_code ec;
        EventLockState s = alpaca::deserialize<EventLockState>(payload, ec);
        if (ec) return;
        post({MsgType::LockState, s.currentState, s.targetState});
      });
  m_nfcEvent = AppEventLoop::subscribe(
      NFC_EVENT, NFC_TAP_EVENT, [this](const uint8_t* data, size_t size) {
        if (size == 0 || data == nullptr) return;
        std::span<const uint8_t> payload(data, size);
        std::error_code ec;
        NfcEvent ev = alpaca::deserialize<NfcEvent>(payload, ec);
        if (ec) return;
        if (ev.type == HOMEKEY_TAP) {
          EventHKTap tap = alpaca::deserialize<EventHKTap>(ev.data, ec);
          if (ec) return;
          post({MsgType::Feedback,
                uint8_t(tap.status ? statusgfx::Feedback::HomeKeyAccepted
                                   : statusgfx::Feedback::HomeKeyRejected),
                0});
        } else if (ev.type == TAG_TAP) {
          post({MsgType::Feedback, uint8_t(statusgfx::Feedback::TagRead), 0});
        }
      });
}

void StatusDisplay::loadSettings() {
  const auto& a = m_config->getConfig<espConfig::actions_config_t>();
  LabelStore fresh = {
      a.displayLabelLocked,   a.displayLabelUnlocked, a.displayLabelLocking,
      a.displayLabelUnlocking, a.displayLabelJammed,  a.displayLabelUnknown,
      a.displayLabelAccepted, a.displayLabelRejected, a.displayLabelTag,
      a.displayLabelIdle,
  };
  Settings settings{a.displayMode, a.displayIdleOff, a.displayTimeout,
                    uint8_t(a.displayRotation & 3)};
  xSemaphoreTake(m_settingsMutex, portMAX_DELAY);
  m_customLabels = std::move(fresh);
  m_settings = settings;
  xSemaphoreGive(m_settingsMutex);
}

StatusDisplay::Settings StatusDisplay::currentSettings() {
  xSemaphoreTake(m_settingsMutex, portMAX_DELAY);
  Settings s = m_settings;
  xSemaphoreGive(m_settingsMutex);
  return s;
}

statusgfx::Labels StatusDisplay::currentLabels(LabelStore& storage) {
  xSemaphoreTake(m_settingsMutex, portMAX_DELAY);
  storage = m_customLabels;
  xSemaphoreGive(m_settingsMutex);
  const statusgfx::Labels& d = statusgfx::defaultLabels(language());
  auto pick = [&](size_t i, const char* fallback) {
    return storage[i].empty() ? fallback : storage[i].c_str();
  };
  return {pick(0, d.locked),   pick(1, d.unlocked), pick(2, d.locking),
          pick(3, d.unlocking), pick(4, d.jammed),  pick(5, d.unknown),
          pick(6, d.accepted),  pick(7, d.rejected), pick(8, d.tag),
          pick(9, d.idle)};
}

void StatusDisplay::announcePower(bool on) {
  const uint8_t v = on ? 1 : 0;
  AppEventLoop::publish(HW_EVENT, HW_DISPLAY_STATE_CHANGED, &v, 1);
}

bool StatusDisplay::begin(uint8_t initialLockState, const ConfigManager& config) {
  m_initialState = initialLockState;
  m_config = &config;
  m_settingsMutex = xSemaphoreCreateMutex();
  if (!m_settingsMutex) return false;
  loadSettings();
  m_rotation = currentSettings().rotation;
  if (!claimPins()) return false;

  m_fb = static_cast<uint16_t*>(heap_caps_malloc(FB_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  m_txDone = xSemaphoreCreateBinary();
  m_queue = xQueueCreate(8, sizeof(Msg));
  if (!m_fb || !m_txDone || !m_queue) {
    ESP_LOGE(TAG, "Out of memory");
    return false;
  }

  // Hardware reset.
  gpio_set_level(PIN_LCD_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level(PIN_LCD_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(120));

  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_LCD_MOSI;
  bus.miso_io_num = -1;
  bus.sclk_io_num = PIN_LCD_SCLK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = FB_BYTES;
  if (esp_err_t err = spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO); err != ESP_OK) {
    ESP_LOGE(TAG, "SPI bus init failed: %s (is SPI3 used by Ethernet?)", esp_err_to_name(err));
    return false;
  }
  m_spiBusOwned = true;

  const Panel panel = probePanel();
  ESP_LOGI(TAG, "AtomS3R panel: %s", panel == Panel::GC9107 ? "GC9107" : "ST7735S");
  if (!initPanel(panel)) return false;
  initBacklight();  // a dark screen is not worth failing over; it is logged

  // Draw the first frame before the backlight comes on.
  statusgfx::Canvas canvas(m_fb, LCD_SIZE, LCD_SIZE, true);
  LabelStore labelStore;
  if (currentSettings().mode == 1) {
    statusgfx::renderIdle(canvas, currentLabels(labelStore));
  } else {
    statusgfx::renderLockState(canvas,
                               statusgfx::displayStateFor(initialLockState, initialLockState),
                               currentLabels(labelStore));
  }
  flush();
  xSemaphoreTake(m_txDone, pdMS_TO_TICKS(200));
  xSemaphoreGive(m_txDone);
  setBrightness(CONFIG_HK_STATUS_DISPLAY_BRIGHTNESS);

  subscribeEvents();
  // Lowest useful priority: the screen must never compete with NFC timing.
  if (xTaskCreate(taskEntry, "status_display", 4096, this, 1, &m_task) != pdPASS) {
    ESP_LOGE(TAG, "Could not start display task");
    return false;
  }
  ESP_LOGI(TAG, "Status display ready");
  return true;
}

void StatusDisplay::taskEntry(void* arg) { static_cast<StatusDisplay*>(arg)->run(); }

void StatusDisplay::run() {
  using namespace statusgfx;
  enum class Power : uint8_t { Awake, Dimmed, Off };

  Canvas canvas(m_fb, LCD_SIZE, LCD_SIZE, true);
  LabelStore labelStore;
  Labels labels = currentLabels(labelStore);
  Settings settings = currentSettings();

  uint8_t current = m_initialState;
  uint8_t shown = displayStateFor(current, current);
  int64_t transitionSince = 0;
  bool overlay = false;
  int64_t overlayUntil = 0;
  int64_t lastActivity = nowMs();
  Power power = Power::Awake;
  bool stickyOn = false;     // MQTT "ON": ignore the inactivity timeout
  bool revealState = false;  // show the lock state in neutral mode until the screen sleeps
  bool lastButton = true;
  announcePower(true);

  auto drawResting = [&] {
    xSemaphoreTake(m_txDone, pdMS_TO_TICKS(200));  // previous frame fully sent
    if (settings.mode == 1 && !revealState) {
      renderIdle(canvas, labels);
    } else {
      renderLockState(canvas, shown, labels);
    }
    flush();
  };

  for (;;) {
    Msg m;
    const bool got = xQueueReceive(m_queue, &m, pdMS_TO_TICKS(50)) == pdTRUE;
    const int64_t now = nowMs();
    bool redraw = false;
    bool wake = false;
    bool turnOff = false;

    if (got) {
      switch (m.type) {
        case MsgType::LockState: {
          current = m.a;
          const uint8_t next = displayStateFor(m.a, m.b);
          if (next != shown) {
            shown = next;
            transitionSince = now;
            redraw = !overlay;
            // Lock changes light the screen only when it shows the state and
            // never goes dark on its own; otherwise a remote lock at night
            // would light it up for anyone outside.
            wake = settings.mode == 0 && !settings.idleOff;
          }
          break;
        }
        case MsgType::Settings:
          labels = currentLabels(labelStore);
          settings = currentSettings();
          m_rotation = settings.rotation;
          overlay = false;
          redraw = true;
          wake = true;  // show the result right away
          break;
        case MsgType::Feedback: {
          const auto fb = static_cast<Feedback>(m.a);
          // Whoever was just let in may see the state until the screen sleeps.
          if (fb == Feedback::HomeKeyAccepted) revealState = true;
          overlay = true;
          overlayUntil = now + CONFIG_HK_STATUS_DISPLAY_FEEDBACK_MS;
          xSemaphoreTake(m_txDone, pdMS_TO_TICKS(200));  // previous frame fully sent
          renderFeedback(canvas, fb, labels);
          flush();
          wake = true;
          break;
        }
        case MsgType::Command:
          switch (m.a) {
            case DISPLAY_CMD_OFF:
              stickyOn = false;
              turnOff = true;
              break;
            case DISPLAY_CMD_ON:
              stickyOn = true;
              wake = true;
              break;
            case DISPLAY_CMD_WAKE_STATE:
              if (!revealState) redraw = true;
              revealState = true;
              stickyOn = false;
              wake = true;
              break;
            case DISPLAY_CMD_WAKE:
              stickyOn = false;
              wake = true;
              break;
            case DISPLAY_CMD_REPORT:
              announcePower(power != Power::Off);
              break;
            default:
              break;
          }
          break;
      }
    }

    if (overlay && now >= overlayUntil) {
      overlay = false;
      redraw = true;
    }

    // If the lock never reports reaching its target, stop showing
    // "locking"/"unlocking" after a while and fall back to the last real state.
    if ((shown == STATE_LOCKING || shown == STATE_UNLOCKING) &&
        now - transitionSince > CONFIG_HK_STATUS_DISPLAY_TRANSITION_TIMEOUT_S * 1000LL) {
      const uint8_t settled = displayStateFor(current, current);
      if (settled != shown) {
        shown = settled;
        redraw = !overlay;
      }
      transitionSince = now;
    }

    const bool button = gpio_get_level(PIN_BUTTON);
    if (lastButton && !button) wake = true;  // pressed (active low)
    lastButton = button;

    if (wake) {
      lastActivity = now;
      if (power == Power::Off) {
        // Paint before the backlight comes on, so the old frame never flashes.
        if (!overlay) drawResting();
        redraw = false;
        announcePower(true);
      }
      if (power != Power::Awake) setBrightness(CONFIG_HK_STATUS_DISPLAY_BRIGHTNESS);
      power = Power::Awake;
    } else if (!turnOff && power == Power::Awake && !stickyOn && settings.timeoutS > 0 &&
               now - lastActivity > settings.timeoutS * 1000LL) {
      if (settings.idleOff) {
        turnOff = true;
      } else {
        power = Power::Dimmed;
        setBrightness(CONFIG_HK_STATUS_DISPLAY_DIM_BRIGHTNESS);
        if (revealState) {
          revealState = false;
          redraw = !overlay;
        }
      }
    }

    if (turnOff && power != Power::Off) {
      setBrightness(0);
      // Also blank the panel itself: nothing readable is left on it.
      xSemaphoreTake(m_txDone, pdMS_TO_TICKS(200));
      canvas.clear(0);
      flush();
      power = Power::Off;
      overlay = false;
      revealState = false;
      redraw = false;
      announcePower(false);
    }

    if (redraw && power != Power::Off) drawResting();
  }
}

#endif  // CONFIG_HK_STATUS_DISPLAY_ATOMS3R
