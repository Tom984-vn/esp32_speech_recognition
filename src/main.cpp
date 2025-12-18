#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <Adafruit_GFX.h>
#include <ArduinoWebsockets.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

using namespace websockets;

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
#define SAMPLING_FREQUENCY 16000
#define DMA_BUF_LEN 256
#define DMA_BUF_COUNT 16
#define I2S_READ_TIMEOUT 20

// ================= STATE =================
enum Mode { MODE_IDLE, MODE_REC };
Mode currentMode = MODE_IDLE;

// ================= WIFI =================
const char* ssid = "xyz";
const char* password = "29122011";

// ================= WS =================
const uint16_t ws_port = 8000;
const char* ws_path = "/ws/audio";
unsigned long lastWSAttempt = 0;

#define FORCED_SERVER_IP "192.168.137.1"

WebsocketsClient client;
volatile bool wsConnected = false;

// ================= WIFI =================
void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[WiFi] Connected");
  Serial.print("[WiFi] ESP IP: ");
  Serial.println(WiFi.localIP());
}

// ================= WS EVENTS =================
void onEvents(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    wsConnected = true;
    Serial.println("[WS] Connected");
  }

  if (event == WebsocketsEvent::ConnectionClosed) {
    wsConnected = false;
    Serial.println("[WS] Disconnected");
  }
}

// ================= WS CONNECT =================
void connectWS() {
  if (wsConnected) return;

  if (millis() - lastWSAttempt < 3000) return;
  lastWSAttempt = millis();
  static bool initialized = false;
  if (!initialized) {
    client.onEvent(onEvents);
    initialized = true;
  }

  Serial.print("[WS] Connecting to ");
  Serial.println(FORCED_SERVER_IP);

  client.close(); 
  delay(200);
  bool ok = client.connect(FORCED_SERVER_IP, ws_port, ws_path);
  if (!ok) {
    Serial.println("[WS] Connect failed");
  }
}


// ================= I2S =================
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
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_setpin();
  i2s_zero_dma_buffer(I2S_PORT);
}

// ================= RECORD =================
void record_audio() {
  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("RECORDING...");
  display.display();

  static int32_t raw[DMA_BUF_LEN];
  static int16_t pcm[DMA_BUF_LEN];
  static uint8_t tx_buffer[sizeof(uint32_t) + DMA_BUF_LEN * sizeof(int16_t)];

  uint32_t packet_seq = 0;
  size_t bytes;

  while (digitalRead(BUTTON_PIN) == LOW) {
    if (!wsConnected) continue;

    client.poll();

    if (i2s_read(I2S_PORT, raw, sizeof(raw), &bytes,
                 I2S_READ_TIMEOUT / portTICK_PERIOD_MS) != ESP_OK) {
      continue;
    }

    int samples = bytes / 4;
    for (int i = 0; i < samples; i++) {
      pcm[i] = raw[i] >> 14;
    }

    memcpy(tx_buffer, &packet_seq, sizeof(uint32_t));
    memcpy(tx_buffer + sizeof(uint32_t),
           pcm, samples * sizeof(int16_t));

    client.sendBinary(
      (const char*)tx_buffer,
      sizeof(uint32_t) + samples * sizeof(int16_t)
    );

    packet_seq++;
  }

  if (wsConnected) {
    client.send("END");
  }

  display.clearDisplay();
  display.setCursor(20, 25);
  display.println("DONE");
  display.display();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(1000);
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);

  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("Initializing...");
  display.display();

  connect_wifi();
  i2s_install_rx();
}

// ================= LOOP =================
void loop() {
  if (wsConnected) {
    client.poll();
  } else {
    connectWS();
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(30);
    if (digitalRead(BUTTON_PIN) == LOW) {
      currentMode = MODE_REC;
      record_audio();
      currentMode = MODE_IDLE;
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    }
  }
  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("Idle...");
  display.display();
}
