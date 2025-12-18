#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <Adafruit_GFX.h>
#include <ArduinoWebsockets.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <string.h>
#include <vector>

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
#define DMA_BUF_LEN 512
#define DMA_BUF_COUNT 16
#define I2S_READ_TIMEOUT 20

// ================= STATE =================
enum Mode { MODE_IDLE, MODE_REC, MODE_PLAY };
Mode currentMode = MODE_IDLE;

// ================= LANGUAGE =================
enum language {en , vi};
language lang = en;

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
// SCREEN
enum Screen {
  SCREEN_MENU,
  SCREEN_RECORD,
  SCREEN_READ,
  SCREEN_SETTINGS
};

Screen currentScreen = SCREEN_MENU;
int Index = 0;
// Early Define
void connectWS();
void record_audio();
void play_audio();

// BUTTON
#define LONG_PRESS 1000

unsigned long pressStart = 0;
bool buttonHeld = false;
bool curr_recplay = false;
int num_respond = 0;

// ================= DATA STORAGE =================
struct SavedResponse {
  String title;
  String content;
};
std::vector<SavedResponse> responseList;
bool isViewingContent = false; // Trạng thái đang đọc nội dung hay đang xem list
int scrollOffset = 0;          // Vị trí cuộn trang hiện tại

// Hàm chuyển đổi Tiếng Việt có dấu sang không dấu
String remove_vi_accents(String str) {
  // A
  str.replace("á", "a"); str.replace("à", "a"); str.replace("ả", "a"); str.replace("ã", "a"); str.replace("ạ", "a");
  str.replace("ă", "a"); str.replace("ắ", "a"); str.replace("ằ", "a"); str.replace("ẳ", "a"); str.replace("ẵ", "a"); str.replace("ặ", "a");
  str.replace("â", "a"); str.replace("ấ", "a"); str.replace("ầ", "a"); str.replace("ẩ", "a"); str.replace("ẫ", "a"); str.replace("ậ", "a");
  str.replace("Á", "A"); str.replace("À", "A"); str.replace("Ả", "A"); str.replace("Ã", "A"); str.replace("Ạ", "A");
  str.replace("Ă", "A"); str.replace("Ắ", "A"); str.replace("Ằ", "A"); str.replace("Ẳ", "A"); str.replace("Ẵ", "A"); str.replace("Ặ", "A");
  str.replace("Â", "A"); str.replace("Ấ", "A"); str.replace("Ầ", "A"); str.replace("Ẩ", "A"); str.replace("Ẫ", "A"); str.replace("Ậ", "A");
  // E
  str.replace("é", "e"); str.replace("è", "e"); str.replace("ẻ", "e"); str.replace("ẽ", "e"); str.replace("ẹ", "e");
  str.replace("ê", "e"); str.replace("ế", "e"); str.replace("ề", "e"); str.replace("ể", "e"); str.replace("ễ", "e"); str.replace("ệ", "e");
  str.replace("É", "E"); str.replace("È", "E"); str.replace("Ẻ", "E"); str.replace("Ẽ", "E"); str.replace("Ẹ", "E");
  str.replace("Ê", "E"); str.replace("Ế", "E"); str.replace("Ề", "E"); str.replace("Ể", "E"); str.replace("Ễ", "E"); str.replace("Ệ", "E");
  // I
  str.replace("í", "i"); str.replace("ì", "i"); str.replace("ỉ", "i"); str.replace("ĩ", "i"); str.replace("ị", "i");
  str.replace("Í", "I"); str.replace("Ì", "I"); str.replace("Ỉ", "I"); str.replace("Ĩ", "I"); str.replace("Ị", "I");
  // O
  str.replace("ó", "o"); str.replace("ò", "o"); str.replace("ỏ", "o"); str.replace("õ", "o"); str.replace("ọ", "o");
  str.replace("ô", "o"); str.replace("ố", "o"); str.replace("ồ", "o"); str.replace("ổ", "o"); str.replace("ỗ", "o"); str.replace("ộ", "o");
  str.replace("ơ", "o"); str.replace("ớ", "o"); str.replace("ờ", "o"); str.replace("ở", "o"); str.replace("ỡ", "o"); str.replace("ợ", "o");
  str.replace("Ó", "O"); str.replace("Ò", "O"); str.replace("Ỏ", "O"); str.replace("Õ", "O"); str.replace("Ọ", "O");
  str.replace("Ô", "O"); str.replace("Ố", "O"); str.replace("Ồ", "O"); str.replace("Ổ", "O"); str.replace("Ỗ", "O"); str.replace("Ộ", "O");
  str.replace("Ơ", "O"); str.replace("Ớ", "O"); str.replace("Ờ", "O"); str.replace("Ở", "O"); str.replace("Ỡ", "O"); str.replace("Ợ", "O");
  // U
  str.replace("ú", "u"); str.replace("ù", "u"); str.replace("ủ", "u"); str.replace("ũ", "u"); str.replace("ụ", "u");
  str.replace("ư", "u"); str.replace("ứ", "u"); str.replace("ừ", "u"); str.replace("ử", "u"); str.replace("ữ", "u"); str.replace("ự", "u");
  str.replace("Ú", "U"); str.replace("Ù", "U"); str.replace("Ủ", "U"); str.replace("Ũ", "U"); str.replace("Ụ", "U");
  str.replace("Ư", "U"); str.replace("Ứ", "U"); str.replace("Ừ", "U"); str.replace("Ử", "U"); str.replace("Ữ", "U"); str.replace("Ự", "U");
  // Y
  str.replace("ý", "y"); str.replace("ỳ", "y"); str.replace("ỷ", "y"); str.replace("ỹ", "y"); str.replace("ỵ", "y");
  str.replace("Ý", "Y"); str.replace("Ỳ", "Y"); str.replace("Ỷ", "Y"); str.replace("Ỹ", "Y"); str.replace("Ỵ", "Y");
  // D
  str.replace("đ", "d"); str.replace("Đ", "D");
  return str;
}

void onShortPress() {
  if (currentScreen == SCREEN_MENU) {
    Index = (Index + 1) % 3;
  } else if (currentScreen == SCREEN_SETTINGS){
    Index = (Index + 1) % 3;
  } else if (currentScreen == SCREEN_RECORD) {
    if (!curr_recplay){
    connectWS();
    record_audio();
    curr_recplay = true;
  } else {
    curr_recplay = false;
  }

  } else if (currentScreen == SCREEN_READ) {
    if (isViewingContent) {
      // [NEW] Chức năng cuộn trang
      String text = remove_vi_accents(responseList[Index].content);
      // Ước tính chiều cao văn bản (6px chiều rộng trung bình, 8px chiều cao dòng)
      int lines = (text.length() * 6) / SCREEN_WIDTH + 1;
      int totalHeight = lines * 8;

      scrollOffset += SCREEN_HEIGHT / 2; // Cuộn xuống nửa màn hình (32px)
      
      // Nếu cuộn quá đáy (trừ đi khoảng trống cuối), quay về đầu
      if (scrollOffset > totalHeight - (SCREEN_HEIGHT / 2)) {
        scrollOffset = 0;
      }
    } else {
      // Nếu đang ở danh sách -> Chuyển sang mục tiếp theo
      // Index chạy từ 0 đến size (size là nút Back)
      int maxItems = responseList.size() + 1; 
      Index = (Index + 1) % maxItems;
    }
  } 
}

void onLongPress() {
  switch (currentScreen) {
    case SCREEN_MENU:
      if (Index == 0) currentScreen = SCREEN_RECORD;
      else if (Index == 1) currentScreen = SCREEN_READ;
      else if (Index == 2) currentScreen = SCREEN_SETTINGS;
      Index = 0;
      break;

    case SCREEN_SETTINGS:
      if (Index == 0) {
        currentScreen = SCREEN_MENU;
        Index = 0;
      } 
      else if (Index == 2) {
        lang = (lang == en) ? vi : en;
      }
      break;

    case SCREEN_RECORD:
      currentScreen = SCREEN_MENU;
      Index = 0;
      break;

    case SCREEN_READ:
      if (isViewingContent) {
        isViewingContent = false; // Quay lại danh sách
        scrollOffset = 0;         // Reset cuộn
      } else {
        // Đang ở danh sách
        if (Index < responseList.size()) {
          isViewingContent = true; // Chọn đọc tin nhắn
        } else {
          // Chọn nút Back
          currentScreen = SCREEN_MENU;
          Index = 0;
        }
      }
      break;
  }
}


void handle_button(){
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  if (pressed && !buttonHeld){
    pressStart = millis();
    buttonHeld = true;
  }
  if (!pressed && buttonHeld){
    unsigned long duration = millis() - pressStart;
    buttonHeld = false;
    if (duration < LONG_PRESS) {
      onShortPress();
    } else  {
      onLongPress();
    } 
  }
}


// UI action handle
void drawMenu() {
  display.clearDisplay();
  display.setCursor(0, 0);

  const char* items[] = {"Record", "Read", "Settings"};

  for (int i = 0; i < 3; i++) {
    if (i == Index) display.print(">");
    else display.print("  ");
    display.println(items[i]);
  }

  display.display();
}

void drawRecord() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Hold to back");
  display.setCursor(20, 25);
  display.println("Press to RECORD");
  display.display();
}

void drawRead() {
  display.clearDisplay();
  display.setCursor(0, 0);

  if (isViewingContent) {
    // Hiển thị nội dung chi tiết
    if (Index < responseList.size()) {
      // [NEW] Xử lý hiển thị nội dung với cuộn và lọc dấu
      String content = remove_vi_accents(responseList[Index].content);
      
      // Đặt con trỏ Y âm để cuộn văn bản lên trên
      display.setCursor(0, -scrollOffset);
      display.println(content);

      // [NEW] Vẽ chỉ báo cuộn (Overlay)
      // Xóa một vùng nhỏ ở góc phải để vẽ mũi tên cho rõ
      if (scrollOffset > 0) {
        display.fillRect(118, 0, 10, 6, BLACK); 
        display.fillTriangle(123, 0, 118, 5, 128, 5, WHITE); // Mũi tên lên
      }
      // Ước tính nếu còn nội dung bên dưới thì vẽ mũi tên xuống
      int lines = (content.length() * 6) / SCREEN_WIDTH + 1;
      if (scrollOffset < (lines * 8) - SCREEN_HEIGHT) {
        display.fillRect(118, 58, 10, 6, BLACK);
        display.fillTriangle(123, 63, 118, 58, 128, 58, WHITE); // Mũi tên xuống
      }
    }
  } else {
    // Hiển thị danh sách Title
    int listSize = responseList.size();
    for (int i = 0; i <= listSize; i++) {
      if (i == Index) display.print(">");
      else display.print("  ");
      
      // Lọc dấu cho cả Title trong danh sách
      if (i < listSize) display.println(remove_vi_accents(responseList[i].title));
      else display.println("Back"); // Mục cuối cùng là Back
    }
    if (listSize == 0) display.println("  (Empty)");
  }
  display.display();
}

void drawSettings() {
  display.clearDisplay();
  display.setCursor(0, 0);
  static char serverIP[50], Language[20];
  snprintf(serverIP, sizeof(serverIP), "Host IP: %s", FORCED_SERVER_IP);
  snprintf(Language, sizeof(Language), "Language: %s", lang == en ? "en" : "vi");
  const char* items[] = {"Back => Menu", serverIP, Language};

  for (int i = 0; i < 3; i++) {
    if (i == Index) display.print(">");
    else display.print("  ");
    display.println(items[i]);
  }
  display.display();
}



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

// ================= MESSAGE HANDLER =================
void handle_message(WebsocketsMessage msg) {
  if (msg.isText()) {
    String data = msg.data();
    
    // Xử lý gói tin RESP:Title|Content
    if (data.startsWith("RESP:")) {
      String payload = data.substring(5);
      int splitIdx = payload.indexOf('|');
      if (splitIdx > 0) {
        String title = payload.substring(0, splitIdx);
        String content = payload.substring(splitIdx + 1);
        
        // Thêm vào đầu danh sách
        responseList.insert(responseList.begin(), {title, content});
        // Giới hạn lưu 5 tin gần nhất
        if (responseList.size() > 5) responseList.pop_back();
      }
    }
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
    client.onMessage(handle_message); // Đăng ký hàm xử lý text mặc định
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

void i2s_install_tx(){
  const i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
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
  currentMode = MODE_REC;
  display.clearDisplay();
  display.setCursor(10, 25);
  delay(10);
  display.println("RECORDING...");
  display.display();

  static int32_t raw[DMA_BUF_LEN];
  static int16_t pcm[DMA_BUF_LEN];
  static uint8_t tx_buffer[sizeof(uint32_t) + DMA_BUF_LEN * sizeof(int16_t)];

  uint32_t packet_seq = 0;
  size_t bytes;

  while (digitalRead(BUTTON_PIN) != LOW) {
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
    String langStr = (lang == en) ? "en" : "vi";
    client.send("END " + String(packet_seq) + " " + langStr);
  }

  display.clearDisplay();
  display.setCursor(20, 25);
  display.println("DONE");
  display.display();
}
//PLAY AUDIO
void play_audio() {
  currentMode = MODE_PLAY;

  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("PLAY WAV...");
  display.display();

  // 1. Chuyển sang I2S TX
  i2s_driver_uninstall(I2S_PORT);
  i2s_install_tx();

  static bool headerSkipped = false;
  static uint32_t totalBytes = 0;

  // 2. Nhận dữ liệu từ WS
  client.onMessage([&](WebsocketsMessage msg) {

    if (msg.isBinary()) {
      const uint8_t* data = (const uint8_t*)msg.c_str();
      size_t len = msg.length();

      size_t offset = 0;

      // Skip WAV header (44 bytes)
      if (!headerSkipped) {
        if (len <= 44) return;
        offset = 44;
        headerSkipped = true;
      }

      size_t written = 0;
      i2s_write(
        I2S_PORT,
        data + offset,
        len - offset,
        &written,
        portMAX_DELAY
      );

      totalBytes += written;
    }

    // END signal
    if (msg.isText() && msg.data() == "END") {
      Serial.println("[PLAY] END received");
      Serial.printf("[PLAY] Played %lu bytes\n", totalBytes);

      headerSkipped = false;
      totalBytes = 0;
      currentMode = MODE_IDLE;
    }
  });

  // 3. Poll khi đang giữ nút (test)
  while (digitalRead(BUTTON_PIN) == LOW) {
    client.poll();
  }

  // 4. Cleanup
  client.onMessage(handle_message); // Khôi phục lại hàm xử lý text
  i2s_driver_uninstall(I2S_PORT);
  i2s_install_rx();

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
  handle_button();

  switch (currentScreen) {
    case SCREEN_MENU: drawMenu(); break;
    case SCREEN_RECORD: drawRecord(); break;
    case SCREEN_READ: drawRead(); break;
    case SCREEN_SETTINGS: drawSettings(); break;
  }

  client.poll();
}
