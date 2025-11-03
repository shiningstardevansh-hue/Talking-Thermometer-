#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2  // DS18B20 data pin connected to D2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorAddr;

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
  sensors.begin();

  // find sensor address (first sensor) and set resolution
  if (sensors.getAddress(sensorAddr, 0)) {
    sensors.setResolution(sensorAddr, RESOLUTION);
    // initialize window
    for (int i = 0; i < MEDIAN_WINDOW; ++i) window[i] = NAN;
    Serial.println("{\"info\":\"ds18b20_initialized\"}");
  } else {
    Serial.println("{\"error\":\"no_ds18b20_found\"}");
  }
}

void loop() {
  unsigned long now = millis();
  if (now - lastRequest < POLL_INTERVAL) return;
  lastRequest = now;

  sensors.requestTemperatures(); // blocking - conversion time depends on resolution
  float t = sensors.getTempC(sensorAddr);
  if (t == DEVICE_DISCONNECTED_C) {
    Serial.println("{\"error\":\"sensor_disconnected\"}");
    return;
  }

  // push to median window
  window[winIdx] = t;
  winIdx = (winIdx + 1) % MEDIAN_WINDOW;
  if (winIdx == 0) windowFilled = true;

  float med = medianOfWindow(window, MEDIAN_WINDOW);
  if (isnan(ema)) ema = med; else ema = EMA_ALPHA * med + (1 - EMA_ALPHA) * ema;

  sendReading(ema);
}
