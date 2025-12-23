#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include "secrets.h"
#include "web_page.h"

// --- Configuration ---
namespace Config {
    constexpr char HOST_NAME[] = "fireplace"; // http://fireplace.local
    
    // Pins
    constexpr int PIN_THERMOCOUPLE = 34; // ADC1_CH6
    constexpr int PIN_WALL_SWITCH = 33;  // RTC Pin for Deep Sleep Wakeup
    constexpr int PIN_VALVE_OUTPUT = 17;
    constexpr int PIN_ONBOARD_LED = 2;
    constexpr int PIN_MASTER_BYPASS = 16; // Active HIGH

    // Safety Thresholds (mV)
    constexpr int THERMO_MIN_MV = 150;
    constexpr int THERMO_MAX_MV = 500;

    // Network (Static IP)
    // Set to false to use DHCP
    constexpr bool USE_STATIC_IP = true;
    // Note: IPAddress cannot be constexpr in Arduino ESP32 core easily without casting, keeping as const
    const IPAddress LOCAL_IP(192, 168, 4, 52);
    const IPAddress GATEWAY(192, 168, 4, 1);
    const IPAddress SUBNET(255, 255, 255, 0);
    const IPAddress PRIMARY_DNS(8, 8, 8, 8);

    // Serial Debug
    constexpr int SERIAL_BAUD = 115200;
}

// --- Global State ---
struct State {
    bool webRequestOn = false;
    unsigned long timerEndTime = 0;
    bool isFireplaceEnabled = false;
    int currentThermoMV = 0;
    bool isWallSwitchActive = false;
    bool isMasterBypassActive = false;
    bool isSafe = false;

};

State state;
WebServer server(80);

// --- Function Prototypes ---
void setupHardware();
void setupWiFi();
void setupServer();
void updateSensors();
void checkTimer();
void updateControlLogic();
void handleRoot();
void handleStatus();
void handleControl();
void handleNotFound();

// --- Main Setup ---
void setup() {
    Serial.begin(Config::SERIAL_BAUD);
    setupHardware();
    setupWiFi();
    setupServer();
}

// --- Main Loop ---
void loop() {
    server.handleClient();
    updateSensors();
    checkTimer();
    updateControlLogic();
    delay(10); // Small delay for stability
}

// --- Initialization Functions ---

void setupHardware() {
    pinMode(Config::PIN_THERMOCOUPLE, INPUT);
    pinMode(Config::PIN_WALL_SWITCH, INPUT_PULLDOWN); // Assume active HIGH
    pinMode(Config::PIN_MASTER_BYPASS, INPUT_PULLDOWN); // Active HIGH
    pinMode(Config::PIN_VALVE_OUTPUT, OUTPUT);
    pinMode(Config::PIN_ONBOARD_LED, OUTPUT);
    
    // Initial state
    digitalWrite(Config::PIN_VALVE_OUTPUT, LOW);
    digitalWrite(Config::PIN_ONBOARD_LED, LOW);
}

void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Disable WiFi power saving for better responsiveness

    if (Config::USE_STATIC_IP) {
        if (!WiFi.config(Config::LOCAL_IP, Config::GATEWAY, Config::SUBNET, Config::PRIMARY_DNS)) {
            Serial.println("Static IP Configuration Failed!");
        }
    }

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
        delay(500);
        Serial.print(".");
        retryCount++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\nConnected! IP: ");
        Serial.println(WiFi.localIP());
        if (MDNS.begin(Config::HOST_NAME)) {
            Serial.printf("mDNS responder started: http://%s.local\n", Config::HOST_NAME);
        }
    } else {
        Serial.println("\nWiFi Connection Failed. Continuing in offline mode (Hardware only).");
    }
}

void setupServer() {
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/control", handleControl);
    server.onNotFound(handleNotFound);
    server.begin();
}

// --- Logic Functions ---

void updateSensors() {
    // Use analogReadMilliVolts for easier conversion on ESP32
    state.currentThermoMV = analogReadMilliVolts(Config::PIN_THERMOCOUPLE);
    state.isMasterBypassActive = (digitalRead(Config::PIN_MASTER_BYPASS) == HIGH);
    state.isWallSwitchActive = (digitalRead(Config::PIN_WALL_SWITCH) == HIGH);
    
    // Safety Check
    state.isSafe = (state.currentThermoMV >= Config::THERMO_MIN_MV 
                        && state.currentThermoMV <= Config::THERMO_MAX_MV);
}

void checkTimer() {
    if (state.timerEndTime > 0 && millis() > state.timerEndTime) {
        state.webRequestOn = false;
        state.timerEndTime = 0;
    }
}

void updateControlLogic() {
    // Fireplace is ON if (Web says ON) AND (Safety is OK).
    // Note: Switch is now just "Master Power". Web controls the valve state.
    bool desiredState = state.webRequestOn;

    if ((desiredState && state.isSafe) || state.isMasterBypassActive) {
        state.isFireplaceEnabled
 = true;
        digitalWrite(Config::PIN_ONBOARD_LED, HIGH);   // Turn on LED to indicate active
        digitalWrite(Config::PIN_VALVE_OUTPUT, HIGH);  // Open valve to allow gas flow
    } else {
        state.isFireplaceEnabled
 = false;
        digitalWrite(Config::PIN_VALVE_OUTPUT, LOW);
        digitalWrite(Config::PIN_ONBOARD_LED, LOW);
    }
}

// --- Web Handlers ---

void handleRoot() {
    // Serial.println("Web: Client connected to /");
    server.send(200, "text/html", index_html);
}

void handleStatus() {
    char json[512];
    long timeLeft = 0;
    if (state.timerEndTime > millis()) {
        timeLeft = (state.timerEndTime - millis()) / 1000;
    }
    snprintf(json, sizeof(json), 
        "{"
        "\"active\":%s,"
        "\"safety\":%s,"
        "\"thermo_mv\":%d,"
        "\"wall_switch\":%s,"
        "\"valve_state\":%d,"
        "\"led_state\":%d,"
        "\"bypass_state\":%d,"
        "\"timer_left\":%ld"
        "}",
        state.isFireplaceEnabled
 ? "true" : "false",
        state.isSafe ? "true" : "false",
        state.currentThermoMV,
        state.isWallSwitchActive ? "true" : "false",
        digitalRead(Config::PIN_VALVE_OUTPUT),
        digitalRead(Config::PIN_ONBOARD_LED),
        digitalRead(Config::PIN_MASTER_BYPASS),
        timeLeft
    );
    
    server.send(200, "application/json", json);
}

void handleControl() {
    Serial.print("Web: Control request");
    
    if (server.hasArg("state")) {
        String stateArg = server.arg("state");
        Serial.printf(" [State: %s]", stateArg.c_str());
        if (stateArg == "on") {
            state.webRequestOn = true;
            state.timerEndTime = 0; // Manual ON cancels timer
        } else if (stateArg == "off") {
            state.webRequestOn = false;
            state.timerEndTime = 0;
        }
    }
    
    if (server.hasArg("timer")) {
        int mins = server.arg("timer").toInt();
        Serial.printf(" [Timer: %dm]", mins);
        if (mins > 0) {
            state.webRequestOn = true; // Timer implies we want it ON
            state.timerEndTime = millis() + (mins * 60000UL);
        } else {
            state.timerEndTime = 0;
        }
    }
    
    Serial.println();
    server.send(200, "text/plain", "OK");
}

void handleNotFound() {
    Serial.printf("Web: 404 Not Found - %s\n", server.uri().c_str());
    server.send(404, "text/plain", "Not found");
}
