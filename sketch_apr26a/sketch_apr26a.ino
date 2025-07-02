#include <WiFi.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Cấu hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Cấu hình Firebase
#define API_KEY "AIzaSyB2cz829BEMrwoxjuMeRF_m_2ibp79DJtw"
#define USER_EMAIL "123@gmail.com"
#define USER_PASSWORD "1234567890"
#define DATABASE_URL "https://duongleproject-default-rtdb.firebaseio.com"

// Đối tượng Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData stream;

// Định nghĩa chân GPIO
#define WIFI_LED 2
#define SOIL_SENSOR 35 // Cảm biến độ ẩm đất
#define DHT_PIN 14 // Cảm biến DHT11
#define RELAY 13 // Relay điều khiển bởi độ ẩm đất
#define LIGHT_SENSOR 34 // Cảm biến ánh sáng
#define TIP122 25 // TIP122 điều khiển bởi ánh sáng
#define BUZZER_PIN 26
#define MODE_LED 15
#define BUTTON_RELAY 32 // Nút bấm điều khiển relay
#define BUTTON_TIP122 33 // Nút bấm điều khiển TIP122
#define BUTTON_MODE 27 // Nút bấm chuyển chế độ

#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// Thông tin WiFi
const char* ssid = "dl";
const char* password = "1234567890";

// Ngưỡng giá trị
const int WET_SOIL = 1050;
const int DRY_SOIL = 3050;
const int MOIST_LOW = 40;
const int MOIST_HIGH = 80;
const int DARK_LIGHT = 500; // Giá trị ánh sáng khi tối
const int BRIGHT_LIGHT = 1500; // Giá trị ánh sáng khi sáng
const int LIGHT_LOW = 60; // Ngưỡng ánh sáng thấp để bật TIP122
const int LIGHT_HIGH = 90; // Ngưỡng ánh sáng cao để tắt TIP122

// Biến hệ thống
int soilValue, lightValue;
int moisture, lightLevel;
int relayState = 0, tip122State = 0;
int autoMode = 1; // Chế độ tự động mặc định
float temp = 0, humi = 0;
bool wifiConnected = false;

// Biến lưu giá trị trước đó để kiểm tra thay đổi
int lastMoisture = -1, lastLightLevel = -1;
float lastTemp = -1, lastHumi = -1;
int lastRelayState = -1, lastTip122State = -1;
int lastMode = -1;
int lastButtonRelayState = -1, lastButtonTip122State = -1;

// Biến thời gian và debounce cho nút bấm
unsigned long lastSensorUpdate = 0;
const long SENSOR_INTERVAL = 3000;
unsigned long lastBuzzerTime = 0;
const long BUZZER_INTERVAL = 3000;
unsigned long lastButtonRelayTime = 0, lastButtonTip122Time = 0, lastButtonModeTime = 0;
const long DEBOUNCE_DELAY = 50;

void setup() {
    Serial.begin(115200);
    
    // Khởi tạo GPIO
    pinMode(WIFI_LED, OUTPUT);
    pinMode(SOIL_SENSOR, INPUT);
    pinMode(LIGHT_SENSOR, INPUT);
    pinMode(RELAY, OUTPUT);
    pinMode(TIP122, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(MODE_LED, OUTPUT);
    pinMode(BUTTON_RELAY, INPUT_PULLUP);
    pinMode(BUTTON_TIP122, INPUT_PULLUP);
    pinMode(BUTTON_MODE, INPUT_PULLUP);
    
    digitalWrite(RELAY, LOW);
    digitalWrite(TIP122, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(MODE_LED, autoMode);
    digitalWrite(WIFI_LED, LOW);

    // Khởi tạo cảm biến
    dht.begin();
    delay(1000);

    // Khởi tạo OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("Lỗi khởi tạo OLED!");
        while(true);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.display();

    // Kết nối WiFi
    connectToWiFi();

    // Khởi động hệ thống
    beep(1000);
    readSensors();
    updateDisplay();

    // Khởi tạo Firebase
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;
    
    Firebase.reconnectNetwork(true);
    fbdo.setBSSLBufferSize(2048, 512);
    Firebase.begin(&config, &auth);
    Serial.println("Đang khởi tạo Firebase...");

    // Khởi tạo stream
    if (Firebase.ready()) {
        if (!Firebase.RTDB.beginStream(&stream, "/")) {
            Serial.println("Lỗi khởi tạo stream: " + stream.errorReason());
        }
        Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
        Serial.println("Đã khởi tạo stream thành công!");
    }
}

void loop() {
    unsigned long currentTime = millis();
    
    // Cập nhật cảm biến và relay/TIP122
    if (currentTime - lastSensorUpdate >= SENSOR_INTERVAL) {
        lastSensorUpdate = currentTime;
        readSensors();
        if (autoMode) {
            controlRelays();
        }
        handleBuzzer();
        updateDisplay();
        sendDataToFirebase();
    }

    // Xử lý nút bấm
    handleButtons();

    // Kiểm tra WiFi
    checkWiFi();

    // Giữ stream hoạt động
    if (Firebase.ready()) {
        Firebase.RTDB.readStream(&stream);
    }
}

void connectToWiFi() {
    Serial.print("Đang kết nối WiFi...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        digitalWrite(WIFI_LED, HIGH);
        Serial.println("\nĐã kết nối! IP: " + WiFi.localIP().toString());
    } else {
        wifiConnected = false;
        Serial.println("\nKhông thể kết nối WiFi!");
    }
}

void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        digitalWrite(WIFI_LED, LOW);
        Serial.println("Mất kết nối WiFi, đang thử lại...");
        connectToWiFi();
    }
}

void readSensors() {
    // Đọc cảm biến độ ẩm đất
    soilValue = analogRead(SOIL_SENSOR);
    moisture = map(soilValue, DRY_SOIL, WET_SOIL, 0, 100);
    moisture = constrain(moisture, 0, 100);
    
    // Đọc cảm biến ánh sáng
    lightValue = analogRead(LIGHT_SENSOR);
    lightLevel = map(lightValue, DARK_LIGHT, BRIGHT_LIGHT, 0, 100);
    lightLevel = constrain(lightLevel, 0, 100);

    // Đọc cảm biến DHT11
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temp = t;
    if (!isnan(h)) humi = h;

    Serial.println("Cảm biến - Độ ẩm đất: " + String(moisture) + "%, Ánh sáng: " + String(lightLevel) + "%, Nhiệt độ: " + String(temp) + "C, Độ ẩm: " + String(humi) + "%");
}

void controlRelays() {
    // Điều khiển relay dựa trên độ ẩm đất
    if (moisture < MOIST_LOW) {
        digitalWrite(RELAY, HIGH);
        relayState = 1;
    } else if (moisture > MOIST_HIGH) {
        digitalWrite(RELAY, LOW);
        relayState = 0;
    }

    // Điều khiển TIP122 dựa trên ánh sáng
    if (lightLevel < LIGHT_LOW) {
        digitalWrite(TIP122, HIGH);
        tip122State = 1;
    } else if (lightLevel > LIGHT_HIGH) {
        digitalWrite(TIP122, LOW);
        tip122State = 0;
    }
}

void handleButtons() {
    // Xử lý nút bấm relay
    int buttonRelayState = digitalRead(BUTTON_RELAY) == LOW ? 1 : 0;
    if (buttonRelayState && (millis() - lastButtonRelayTime) > DEBOUNCE_DELAY) {
        lastButtonRelayTime = millis();
        if (!autoMode) {
            relayState = !relayState;
            digitalWrite(RELAY, relayState);
            Serial.println("Nút bấm relay: " + String(relayState));
            updateDisplay();
            // Gửi trạng thái nút và relay lên Firebase
            if (Firebase.ready()) {
                if (Firebase.RTDB.setInt(&fbdo, "/buttons/1", buttonRelayState)) {
                    Serial.println("Cập nhật nút bấm relay lên Firebase: " + String(buttonRelayState));
                }
                if (Firebase.RTDB.setInt(&fbdo, "/relays/1", relayState)) {
                    Serial.println("Cập nhật relay lên Firebase: " + String(relayState));
                    lastRelayState = relayState;
                }
                // Gửi trạng thái nút thả sau 50ms
                delay(50);
                if (Firebase.RTDB.setInt(&fbdo, "/buttons/1", 0)) {
                    Serial.println("Cập nhật nút bấm relay thả lên Firebase: 0");
                }
            }
        }
    }

    // Xử lý nút bấm TIP122
    int buttonTip122State = digitalRead(BUTTON_TIP122) == LOW ? 1 : 0;
    if (buttonTip122State && (millis() - lastButtonTip122Time) > DEBOUNCE_DELAY) {
        lastButtonTip122Time = millis();
        if (!autoMode) {
            tip122State = !tip122State;
            digitalWrite(TIP122, tip122State);
            Serial.println("Nút bấm TIP122: " + String(tip122State));
            updateDisplay();
            // Gửi trạng thái nút và TIP122 lên Firebase
            if (Firebase.ready()) {
                if (Firebase.RTDB.setInt(&fbdo, "/buttons/2", buttonTip122State)) {
                    Serial.println("Cập nhật nút bấm TIP122 lên Firebase: " + String(buttonTip122State));
                }
                if (Firebase.RTDB.setInt(&fbdo, "/relays/2", tip122State)) {
                    Serial.println("Cập nhật TIP122 lên Firebase: " + String(tip122State));
                    lastTip122State = tip122State;
                }
                // Gửi trạng thái nút thả sau 50ms
                delay(50);
                if (Firebase.RTDB.setInt(&fbdo, "/buttons/2", 0)) {
                    Serial.println("Cập nhật nút bấm TIP122 thả lên Firebase: 0");
                }
            }
        }
    }

    // Xử lý nút bấm chuyển chế độ
    if (digitalRead(BUTTON_MODE) == LOW && (millis() - lastButtonModeTime) > DEBOUNCE_DELAY) {
        lastButtonModeTime = millis();
        autoMode = !autoMode;
        digitalWrite(MODE_LED, autoMode);
        Serial.println("Chuyển chế độ: " + String(autoMode ? "TỰ ĐỘNG" : "THỦ CÔNG"));
        if (autoMode) {
            controlRelays(); // Cập nhật relay và TIP122
            // Đặt trạng thái nút bấm về 0 trên Firebase
            if (Firebase.ready()) {
                Firebase.RTDB.setInt(&fbdo, "/buttons/1", 0);
                Firebase.RTDB.setInt(&fbdo, "/buttons/2", 0);
                Serial.println("Đặt trạng thái nút bấm về 0 trên Firebase");
            }
        }
        updateDisplay();
        // Cập nhật chế độ và trạng thái relay/TIP122 lên Firebase
        if (Firebase.ready()) {
            if (Firebase.RTDB.setInt(&fbdo, "/system/mode", autoMode)) {
                Serial.println("Cập nhật chế độ lên Firebase: " + String(autoMode));
                lastMode = autoMode;
            }
            sendDataToFirebase(); // Gửi trạng thái relay/TIP122
        }
    }
}

void sendDataToFirebase() {
    if (!Firebase.ready()) return;

    // Gửi độ ẩm đất nếu thay đổi quá 5%
    if (abs(moisture - lastMoisture) >= 5 || lastMoisture == -1) {
        if (Firebase.RTDB.setInt(&fbdo, "/sensors/1/moisture", moisture)) {
            Serial.println("Cập nhật độ ẩm đất: " + String(moisture));
            lastMoisture = moisture;
        }
    }
    
    // Gửi mức ánh sáng nếu thay đổi quá 5%
    if (abs(lightLevel - lastLightLevel) >= 5 || lastLightLevel == -1) {
        if (Firebase.RTDB.setInt(&fbdo, "/sensors/2/light", lightLevel)) {
            Serial.println("Cập nhật ánh sáng: " + String(lightLevel));
            lastLightLevel = lightLevel;
        }
    }

    // Gửi nhiệt độ nếu thay đổi quá 1 độ
    if (abs(temp - lastTemp) >= 1 || lastTemp == -1) {
        if (Firebase.RTDB.setFloat(&fbdo, "/sensors/1/temperature", temp)) {
            Serial.println("Cập nhật nhiệt độ: " + String(temp));
            lastTemp = temp;
        }
    }
    
    // Gửi độ ẩm không khí nếu thay đổi quá 5%
    if (abs(humi - lastHumi) >= 5 || lastHumi == -1) {
        if (Firebase.RTDB.setFloat(&fbdo, "/sensors/1/humidity", humi)) {
            Serial.println("Cập nhật độ ẩm không khí: " + String(humi));
            lastHumi = humi;
        }
    }

    // Gửi trạng thái relay nếu có thay đổi
    if (relayState != lastRelayState || lastRelayState == -1) {
        if (Firebase.RTDB.setInt(&fbdo, "/relays/1", relayState)) {
            Serial.println("Cập nhật relay: " + String(relayState));
            lastRelayState = relayState;
        }
    }
    
    // Gửi trạng thái TIP122 nếu có thay đổi
    if (tip122State != lastTip122State || lastTip122State == -1) {
        if (Firebase.RTDB.setInt(&fbdo, "/relays/2", tip122State)) {
            Serial.println("Cập nhật TIP122: " + String(tip122State));
            lastTip122State = tip122State;
        }
    }
}

void streamCallback(FirebaseStream data) {
    if (!Firebase.ready()) return;

    String path = data.dataPath();
    Serial.println("Stream nhận được dữ liệu tại: " + path);

    // Xử lý thay đổi chế độ
    if (path == "/system/mode") {
        autoMode = data.to<int>();
        digitalWrite(MODE_LED, autoMode);
        Serial.println("Chế độ cập nhật từ Firebase: " + String(autoMode ? "TỰ ĐỘNG" : "THỦ CÔNG"));
        if (autoMode) {
            controlRelays();
            // Đặt trạng thái nút bấm về 0 trên Firebase
            if (Firebase.ready()) {
                Firebase.RTDB.setInt(&fbdo, "/buttons/1", 0);
                Firebase.RTDB.setInt(&fbdo, "/buttons/2", 0);
                Serial.println("Đặt trạng thái nút bấm về 0 trên Firebase");
            }
            sendDataToFirebase();
        }
        updateDisplay();
    }
    // Xử lý thay đổi relay/TIP122 ở chế độ thủ công
    else if (path.startsWith("/relays/") && !autoMode) {
        if (path == "/relays/1") {
            relayState = data.to<int>();
            digitalWrite(RELAY, relayState);
            Serial.println("Relay cập nhật từ Firebase: " + String(relayState));
            updateDisplay();
            lastRelayState = relayState;
        } else if (path == "/relays/2") {
            tip122State = data.to<int>();
            digitalWrite(TIP122, tip122State);
            Serial.println("TIP122 cập nhật từ Firebase: " + String(tip122State));
            updateDisplay();
            lastTip122State = tip122State;
        }
    }
}

void streamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.println("Stream timeout, đang thử kết nối lại...");
        if (Firebase.ready()) {
            Firebase.RTDB.beginStream(&stream, "/");
        }
    }
}

void handleBuzzer() {
    if (autoMode && moisture < MOIST_LOW) {
        unsigned long now = millis();
        if (now - lastBuzzerTime >= BUZZER_INTERVAL) {
            lastBuzzerTime = now;
            digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        }
    } else {
        digitalWrite(BUZZER_PIN, LOW);
    }
}

void updateDisplay() {
    display.clearDisplay();
    
    display.setCursor(0, 0);
    display.print("K1:");
    display.print(moisture);
    display.print("% ");
    display.print(relayState ? "ON " : "OFF");

    display.setCursor(70, 0);
    display.print("T1:");
    display.print(temp, 1);
    display.print("C");

    display.setCursor(0, 12);
    display.print("L2:");
    display.print(lightLevel);
    display.print("% ");
    display.print(tip122State ? "ON " : "OFF");

    display.setCursor(70, 12);
    display.print("H1:");
    display.print(humi, 1);
    display.print("%");

    display.setCursor(0, 24);
    display.print("Che do: ");
    display.print(autoMode ? "TU DONG" : "THU CONG");
    display.display();
}

void beep(int duration) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
}