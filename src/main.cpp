#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 8
#define OLED_SCL 9
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= PINS =================
#define BUTTON_PIN  3
#define I2S_SCK     4
#define I2S_WS      5
#define I2S_SD_IN   6
#define I2S_SD_OUT  7
#define I2S_PORT    I2S_NUM_0

// ================= AUDIO =================
#define SAMPLING_FREQUENCY 44100

// ================= STATE =================
enum Mode {MODE_IDLE, MODE_REC, MODE_PLAY };
Mode currentMode = MODE_IDLE;

const char* ssid = "Phong 400";
const char* password = "Aephong400";

// ================= BUFFER =================
// const int TOTAL_SAMPLES = SAMPLING_FREQUENCY * RECORD_TIME;
int16_t *recording_buffer;

//Wifi
void connect_wifi(){
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
}

// I2S func
void i2s_uninstall() {
  i2s_driver_uninstall(I2S_PORT);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_SD_OUT,
    .data_in_num = I2S_SD_IN
  };
  i2s_set_pin(I2S_PORT, &pin_config);
}

void i2s_install_rx() {
  const i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLING_FREQUENCY,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_setpin();
  i2s_zero_dma_buffer(I2S_PORT);
}

void i2s_install_tx() {
  const i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLING_FREQUENCY,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_setpin();
  i2s_zero_dma_buffer(I2S_PORT);
}

// OLED
void run_text_recognizer() {
  display.clearDisplay();

  display.display();
}


// =================================================
// ================= RECORD =========================
// =================================================
void record_audio() {
  display.clearDisplay();
  display.setCursor(20, 25);
  display.println("RECORDING...");
  display.display();

  int samples = 0;
  int32_t temp[64];
  size_t bytes;

  while (digitalRead(BUTTON_PIN) == LOW) {
    i2s_read(I2S_PORT, temp, sizeof(temp), &bytes, portMAX_DELAY);
    int count = bytes / 4;
/*
    for (int i = 0; i < count && samples < TOTAL_SAMPLES; i++) {
      recording_buffer[samples++] = (int16_t)(temp[i] >> 14);
    }
  }
*/
  }

}

// =================================================
// ================= PLAY ===========================
// =================================================
void play_audio(int samples) {
  display.clearDisplay();
  display.setCursor(30, 25);
  display.println("PLAYING...");
  display.display();

  int32_t temp[64];
  size_t bytes;
  int idx = 0;

  while (idx < samples) {
    int n = 0;
    while (n < 64 && idx < samples) {
      temp[n++] = ((int32_t)recording_buffer[idx++]) << 14;
    }
    i2s_write(I2S_PORT, temp, n * 4, &bytes, portMAX_DELAY);
  }
}

// SETUP

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  connect_wifi();
  recording_buffer = (int16_t *)heap_caps_malloc(
    TOTAL_SAMPLES * sizeof(int16_t), MALLOC_CAP_8BIT
  );

  i2s_install_rx();
}


// =================================================
// ================= LOOP ===========================
// =================================================

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      // === RECORD ===
      currentMode = MODE_REC;
      i2s_uninstall();
      i2s_install_rx();
      record_audio();

      // === PLAY ===
      currentMode = MODE_PLAY;
      i2s_uninstall();
      i2s_install_tx();
      play_audio();

      // === BACK TO VIS ===
      currentMode = MODE_IDLE;
      i2s_uninstall();
      i2s_install_rx();

      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    }
  }

}
