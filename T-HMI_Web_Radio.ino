#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Audio.h>
#include <TFT_eSPI.h>   // TFT display library
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>  // Biblioteca pentru stocarea in NVS (non-volatile storage)

// touch screen
#define SCLK_PIN 1
#define MISO_PIN 4
#define MOSI_PIN 3
#define CS_PIN   2
#define TIRQ_PIN  9

// Power pin for the touchscreen
#define PWR_EN_PIN  (10)
#define PWR_ON_PIN  (14)

//WiFi Connection
WiFiManager myWiFi;

// Radio URLs
const char* radioURLs[] = {
  "http://stream.otvoreni.hr/otvoreni",
  "http://live.electricfm.com/electricfm",
  "http://stream2.radiotransilvania.ro/Oradea",
  "http://stream2.radiotransilvania.ro/Nagyvarad",
  "http://icecast.luxnet.ua/lux",
  "http://nl.ah.fm/live",
  "http://s4-webradio.rockantenne.de/90er-rock/stream/mp3",
  "https://punk.stream.laut.fm/punk",
  "http://edge76.rdsnet.ro:84/digifm/digifm.mp3",
  "http://edge126.rdsnet.ro:84/profm/profm.mp3",
  "http://edge126.rdsnet.ro:84/profm/dancefm.mp3",
  "http://144.76.106.52:7000/chillout.mp3",
  "https://streaming.radiostreamlive.com/radionylive_devices",
  "http://stream.104.6rtl.com/dance-hits/mp3-192/"

};

//Constant definitions
const int numberOfChannels = sizeof(radioURLs) / sizeof(radioURLs[0]);
int currentChannel = 0;
int volume = 5;

// Preferences object for saving the last station
Preferences preferences;

// TFT display and touch screen instances
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN); 

// I2S pin configuration for ESP32
#define I2S_DOUT 43
#define I2S_BCLK 44
#define I2S_LRC 18

// Color constants
#define TFT_DARKBLUE  0x0000
#define TFT_DARKGREEN 0x03E0

// Audio object for decoding
Audio audio;

// Draw a button on the screen
void drawButton(int x, int y, int width, int height, const char* label, uint16_t color) {
  tft.fillRect(x, y, width, height, color);
  tft.setCursor(x + 25, y + 25);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  tft.println(label);
}

// Draw all buttons on the screen
void drawButtons() {
  drawButton(10, 200, 100, 50, "Prev", TFT_BLUE);
  drawButton(130, 200, 100, 50, "Next", TFT_BLUE);
  drawButton(10, 260, 100, 50, "Vol -", TFT_DARKGREEN);
  drawButton(130, 260, 100, 50, "Vol +", TFT_DARKGREEN);
}

// Display current volume level
void displayVolume() {
  tft.fillRect(0, 160, tft.width(), 40, TFT_BLACK);  // Clear volume area
  tft.setCursor(10, 175);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  tft.printf("Volume: %d\n", volume);
}

// Connect to the current radio stream
void connectToRadio(const char* url) {
  Serial.print("Connecting to radio: ");
  Serial.println(url);
  audio.connecttohost(url);
}

// Change the radio channel
void changeChannel(int direction) {
  currentChannel = (currentChannel + direction + numberOfChannels) % numberOfChannels;  // Cycle channels

  // Save the current channel index to NVS
  preferences.begin("radio", false);  // Open in read-write mode
  preferences.putInt("lastStation", currentChannel);  // Save the current channel
  preferences.end();  // Close the preferences

  connectToRadio(radioURLs[currentChannel]);
}

// Handle touch events
void handleTouchEvent(int x, int y) {

  if (x >= 2200 && x <= 3600 && y >= 2400 && y <= 2900) {
    changeChannel(-1); // Prev channel
    drawButton(10, 200, 100, 50, "Prev", TFT_DARKBLUE); // Feedback
  } else if (x >= 450 && x <= 1850 && y >= 2400 && y <= 2900) {
    changeChannel(1); // Next channel
    drawButton(130, 200, 100, 50, "Next", TFT_DARKBLUE); // Feedback
  } else if (x >= 2200 && x <= 3600 && y >= 3000 && y <= 3600) {
    if (volume > 0) volume--;  // Decrease volume
    audio.setVolume(volume);
    drawButton(10, 260, 100, 50, "Vol -", TFT_DARKGREEN); // Feedback
    displayVolume();
  } else if (x >= 450 && x <= 1850 && y >= 3000 && y <= 3600) {
    if (volume < 21) volume++;  // Increase volume
    audio.setVolume(volume);
    drawButton(130, 260, 100, 50, "Vol +", TFT_DARKGREEN); // Feedback
    displayVolume();
  }
}

// Check for touch input and handle it
void checkTouch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    // Map the touch coordinates to the screen coordinates
    int x = map(p.x, 0, 240, 0, tft.width());
    int y = map(p.y, 0, 320, 0, tft.height());

    delay(100);

    handleTouchEvent(x, y);
    drawButtons();  // Redraw buttons after feedback
  }
}

// Handle metadata from audio stream
void audio_info(const char* info) {
  Serial.print("info: ");
  Serial.println(info);

  const char* nameTag = "icy-name:";
  char* nameStart = strstr((char*)info, nameTag);
  if (nameStart) {
    nameStart += strlen(nameTag);
    char* nameEnd = strstr(nameStart, "\n");
    if (nameEnd) *nameEnd = '\0';
    Serial.print("Channel Name: ");
    Serial.println(nameStart);

    tft.fillRect(0, 20, tft.width(), 50, TFT_BLACK);  // Clear area
    tft.setCursor(0, 30);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeMonoBold9pt7b);
    tft.println(nameStart);
  }

  const char* titleTag = "StreamTitle=";
  char* titleStart = strstr((char*)info, titleTag);
  if (titleStart) {
    titleStart += strlen(titleTag);
    char* titleEnd = strstr(titleStart, ";");
    if (titleEnd) *titleEnd = '\0';
    Serial.print("Stream Title: ");
    Serial.println(titleStart);

    tft.fillRect(0, 80, tft.width(), 70, TFT_BLACK);  // Clear area
    tft.setCursor(0, 90);
    tft.setTextColor(TFT_YELLOW);
    tft.setFreeFont(&FreeMono9pt7b);
    tft.println(titleStart);
  }
}
// Show message for no Wi-Fi connection
void show_Message_No_Connection(WiFiManager* myWiFi) {
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, 10);
  tft.setFreeFont(&FreeMono9pt7b);
  tft.println(F("WiFi: no connection.\nConnect to hotspot 'My_Radio'\nand open a browser\nat 192.168.4.1\nto enter network credentials."));
}

// Setup function
void setup() {
  // Power pin enable
  pinMode(PWR_ON_PIN, OUTPUT);
  digitalWrite(PWR_ON_PIN, HIGH);
  
  Serial.begin(115200);
  delay(1000);

  pinMode(PWR_EN_PIN, OUTPUT);
  digitalWrite(PWR_EN_PIN, HIGH);

  // SPI initialization for touch screen
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN);
  ts.begin();

  ts.setRotation(0);
  while (!Serial && (millis() <= 1000));

  // TFT display setup
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  // Wi-Fi setup
  myWiFi.setAPCallback(show_Message_No_Connection);
  myWiFi.autoConnect("My_Radio");

  // Initialize Preferences
  preferences.begin("radio", true);  // "radio" is the namespace, true means read-only mode

  // Load the last saved station index (default to 0 if not set)
  currentChannel = preferences.getInt("lastStation", 0);
  preferences.end();  // Close the preferences to save memory

  // I2S setup for audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);

  // Connect to the last saved radio station
  connectToRadio(radioURLs[currentChannel]);

  // Draw buttons on screen
  drawButtons();
  displayVolume();
}

// Main loop function
void loop() {
  audio.loop();  // Audio playback loop
  checkTouch();  // Check for touch inputs
}

