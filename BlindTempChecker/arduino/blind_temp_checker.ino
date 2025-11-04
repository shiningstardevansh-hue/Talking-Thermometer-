#if __has_include(<OneWire.h>)
#include <OneWire.h>
#else
// Minimal OneWire stub for editor/CI when library is not available
class OneWire {
public:
  explicit OneWire(int pin) {}
};
#endif

#if __has_include(<DallasTemperature.h>)
#include <DallasTemperature.h>
#else
// Minimal DallasTemperature stub for editor/CI when library is not available
#include <math.h>
using DeviceAddress = uint8_t[8];
static constexpr float DEVICE_DISCONNECTED_C = -127.0f;
class DallasTemperature {
public:
  explicit DallasTemperature(OneWire* wire) {}
  void begin() {}
  bool getAddress(DeviceAddress& addr, int index) { (void)addr; (void)index; return false; }
  void setResolution(const DeviceAddress& addr, int resolution) { (void)addr; (void)resolution; }
  void requestTemperatures() {}
  float getTempC(const DeviceAddress& addr) { (void)addr; return DEVICE_DISCONNECTED_C; }
};
#endif

#if __has_include(<EEPROM.h>)
#include <EEPROM.h>
#endif

#define ONE_WIRE_BUS 2  // DS18B20 data pin connected to D2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
// Support multiple sensors
const int MAX_SENSORS = 6;
DeviceAddress sensorAddrs[MAX_SENSORS];
DeviceAddress sensorAddr;
int sensorCount = 0;

// Calibration storage in EEPROM: for each sensor store float offset (4 bytes) and multiplier (4 bytes)
// Layout: sensor 0 offset@0..3, mult@4..7, sensor1 offset@8..11, etc.
const int EEPROM_BASE = 0;

struct Cal { float offset; float mult; };

Cal getCalibration(int idx) {
  Cal c;
  int addr = EEPROM_BASE + idx * sizeof(Cal);
  EEPROM.get(addr, c);
  // if uninitialized, fallback to zero offset, multiplier 1
  if (isnan(c.mult) || c.mult == 0) { c.offset = 0.0; c.mult = 1.0; }
  return c;
}

void setCalibration(int idx, Cal c) {
  int addr = EEPROM_BASE + idx * sizeof(Cal);
  EEPROM.put(addr, c);
  EEPROM.commit();
}

// Accuracy / speed tradeoffs: 12-bit is most precise (750ms conversion).
// We'll use a small median window + exponential moving average to reduce noise
// while preserving responsiveness.
const int RESOLUTION = 12;         // 9-12 bits (12 is highest resolution)
const unsigned long POLL_INTERVAL = 1000; // ms between requests (>= conversion time)

const int MEDIAN_WINDOW = 5; // keep a small window for median filter
float window[MEDIAN_WINDOW];
int winIdx = 0;
bool windowFilled = false;

float ema = NAN;
const float EMA_ALPHA = 0.25; // smoothing factor for EMA (0-1)

unsigned long lastRequest = 0;

// Helper: compute median of valid entries in array (ignores NAN)
float medianOfWindow(float arr[], int n) {
  float tmp[MEDIAN_WINDOW];
  int c = 0;
  for (int i = 0; i < n; ++i) {
    if (!isnan(arr[i])) tmp[c++] = arr[i];
  }
  if (c == 0) return NAN;
  // simple insertion sort for small c
  for (int i = 1; i < c; ++i) {
    float key = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > key) {
      tmp[j + 1] = tmp[j];
      j--;
    }
    tmp[j + 1] = key;
  }
  return tmp[c / 2];
}

void sendReading(float value) {
  if (isnan(value) || value == DEVICE_DISCONNECTED_C) {
    Serial.println("{\"error\":\"sensor_disconnected\"}");
    return;
  }
  float f = value * 9.0 / 5.0 + 32.0;
  unsigned long ts = millis();
  // JSON line per reading (newline terminated)
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"tempC\":%.4f,\"tempF\":%.4f,\"ts\":%lu}", value, f, ts);
  Serial.println(buf);
}

void setup() {
  // use a higher baud to transfer readings faster and reliably
  Serial.begin(115200);
  // EEPROM for calibration: required for platforms like ESP; for AVR need EEPROM.begin not present
  #if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
    EEPROM.begin(512);
  #endif
  sensors.begin();

  // discover sensors
  sensorCount = sensors.getDeviceCount();
  if (sensorCount > MAX_SENSORS) sensorCount = MAX_SENSORS;
  for (int i = 0; i < sensorCount; ++i) {
    if (sensors.getAddress(sensorAddrs[i], i)) {
      sensors.setResolution(sensorAddrs[i], RESOLUTION);
    }
  }
  // initialize window
  for (int i = 0; i < MEDIAN_WINDOW; ++i) window[i] = NAN;
  Serial.print("{\"info\":\"ds18b20_initialized\",\"count\":");
  Serial.print(sensorCount);
  Serial.println("}");
}

void loop() {
  unsigned long now = millis();
  if (now - lastRequest < POLL_INTERVAL) return;
  lastRequest = now;

  // request temperatures then read each sensor and send per-sensor reading
  sensors.requestTemperatures(); // blocking - conversion time depends on resolution
  for (int s = 0; s < sensorCount; ++s) {
    float t = sensors.getTempC(sensorAddrs[s]);
    if (t == DEVICE_DISCONNECTED_C) {
      // emit error per sensor
      char buf[128];
      snprintf(buf, sizeof(buf), "{\"error\":\"sensor_disconnected\",\"id\":%d}", s);
      Serial.println(buf);
      continue;
    }

    // per-sensor window/EMA: a simple per-sensor EMA using shared window as tiny circular buffer per sensor would be better
    // For simplicity keep single EMA per sensor by storing last in EEPROM-like RAM arrays
    static float lastEma[MAX_SENSORS];
    // push into window (reuse small per-sensor rolling mechanism)
    static float winBuf[MAX_SENSORS][MEDIAN_WINDOW];
    static int winPos[MAX_SENSORS] = {0};
    winBuf[s][winPos[s]] = t;
    winPos[s] = (winPos[s] + 1) % MEDIAN_WINDOW;
    float med = medianOfWindow(winBuf[s], MEDIAN_WINDOW);
    if (isnan(lastEma[s])) lastEma[s] = med; else lastEma[s] = EMA_ALPHA * med + (1 - EMA_ALPHA) * lastEma[s];

    // apply calibration
    Cal cal = getCalibration(s);
    float calibrated = lastEma[s] * cal.mult + cal.offset;

    // include sensor id in sendReading (overload by printing JSON here)
    if (isnan(calibrated)) {
      char buf[128];
      snprintf(buf, sizeof(buf), "{\"error\":\"no_valid_reading\",\"id\":%d}", s);
      Serial.println(buf);
    } else {
      float f = calibrated * 9.0 / 5.0 + 32.0;
      unsigned long ts = millis();
      char buf[192];
      // include sensor address as hex
      char addrHex[48] = "";
      for (uint8_t i = 0; i < 8; i++) {
        char part[4];
        snprintf(part, sizeof(part), "%02X", sensorAddrs[s][i]);
        strncat(addrHex, part, sizeof(addrHex) - strlen(addrHex) - 1);
      }
      snprintf(buf, sizeof(buf), "{\"id\":%d,\"addr\":\"%s\",\"tempC\":%.4f,\"tempF\":%.4f,\"ts\":%lu}", s, addrHex, calibrated, f, ts);
      Serial.println(buf);
    }
  }
}

// Listen for serial commands to set calibration, e.g.:
// SETCAL IDX OFFSET MULT
// Example: SETCAL 0 0.5 1.0  (set sensor 0 offset +0.5°C, multiplier 1.0)
void processSerialCommands() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (line.startsWith("SETCAL")) {
    int idx = -1; float offset = 0; float mult = 1;
    int matched = sscanf(line.c_str(), "SETCAL %d %f %f", &idx, &offset, &mult);
    if (matched >= 2 && idx >= 0 && idx < MAX_SENSORS) {
      Cal c; c.offset = offset; c.mult = (matched==3?mult:1.0);
      setCalibration(idx, c);
      Serial.print("{\"info\":\"calibration_set\",\"id\":"); Serial.print(idx);
      Serial.print(",\"offset\":"); Serial.print(offset);
      Serial.print(",\"mult\":"); Serial.print(c.mult);
      Serial.println("}");
    } else {
      Serial.println("{\"error\":\"bad_setcal\"}");
    }
  }
}
