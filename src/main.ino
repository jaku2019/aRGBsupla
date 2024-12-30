#include <Arduino.h>
/*
  Copyright (C) AC SOFTWARE SP. Z O.O.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <SPI.h>
#include <SuplaDevice.h>

#include <supla/control/virtual_relay.h>
#include <supla/control/rgbw_base.h>
#include <supla/control/button.h>


// ESP8266 based board:
#include <supla/network/esp_wifi.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/device/supla_ca_cert.h>

#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;

#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>

// Inputy Supla
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/network/html/custom_parameter.h>
#include <supla/network/html/custom_text_parameter.h>
#include <supla/network/html/text_cmd_input_parameter.h>
#include <supla/network/html/select_input_parameter.h>

#include <FastLED.h>

#define STATUS_LED_GPIO 2

// Domyślny numer pinu CONFIG
#ifndef CONFIG_PIN
#define CONFIG_PIN 25
#endif

// Domyślny numer pinu LED_PIN (zostanie podmieniony po restarcie, jeśli zmieni się w CONFIG-u)
#ifndef LED_PIN
#define LED_PIN 16
#endif

#define COLOR_ORDER GRB
#define CHIPSET     WS2812B
// Domyślna liczba diod LED (zostanie podmieniona po restarcie, jeśli zmieni się w CONFIG-u)
#ifndef NUM_LEDS
#define NUM_LEDS 7
#endif

#define RED_PIN              101
#define GREEN_PIN            102
#define BLUE_PIN             103
#define COLOR_BRIGHTNESS_PIN 104
#define BRIGHTNESS_PIN       105

#define BRIGHTNESS  150
#define FRAMES_PER_SECOND 60
#define FASTLED_ALLOW_INTERRUPTS 0
FASTLED_USING_NAMESPACE
#define TEMPERATURE_1 Tungsten100W
#define TEMPERATURE_2 OvercastSky
#define DISPLAYTIME 30
bool gReverseDirection = false;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
CRGB leds[NUM_LEDS];
CRGBPalette16 gPal;

Supla::Control::RGBWBase *r_g_b_w = nullptr;
Supla::Control::VirtualRelay *virtual_relay1 = nullptr;  // name for relay 1
Supla::Control::VirtualRelay *virtual_relay2 = nullptr;  // name for relay 2
Supla::Control::VirtualRelay *virtual_relay3 = nullptr;  // name for relay 3
Supla::Control::VirtualRelay *virtual_relay4 = nullptr;  // name for relay 4
Supla::Control::VirtualRelay *virtual_relay5 = nullptr;  // name for relay 5

// zmienne pomocnicze //
int X0, X1, X2, X3, X4, X5 = false;
int FUNKCJA = 100;
int R_G_B_W = false;

// parametry do ustawiania przez CONFIG
const char DEV_NAME[] = "aRGBsupla";
char devName[30] = {};
const char LED_PIN_PARAM[] = "led_pin"; // Nazwa parametru dla pola listy wyboru pinu DIN
const char NUM_LEDS_PARAM[] = "num_leds"; // Nazwa parametru dla liczby LED
const char CONFIG_PIN_PARAM[] = "config_pin"; // Nazwa parametru dla pinu przycisku


void nextMode() {
  if (FUNKCJA > 50) {
    Serial.println("currentMode > 50, brak akcji.");
    return;
  }

  // Przełącz na kolejny tryb (5 trybów: 1, 2, 3, 4, 5)
  FUNKCJA = FUNKCJA % 5 + 1;
  Serial.printf("Zmieniono tryb na: %d\n", FUNKCJA);

  // Funkcje z biblioteki FastLed oraz sterowanie virtual_relay
  switch (FUNKCJA) {
    case 0:
      FUNKCJA = 100;
      OFF();
      break;
    case 1:
      virtual_relay1->turnOn();
      virtual_relay2->turnOff();
      virtual_relay3->turnOff();
      virtual_relay4->turnOff();
      virtual_relay5->turnOff();
      Confetti();
      FastLED.show();
      FastLED.delay(1000 / FRAMES_PER_SECOND);
      break;
    case 2:
      virtual_relay1->turnOff();
      virtual_relay2->turnOn();
      virtual_relay3->turnOff();
      virtual_relay4->turnOff();
      virtual_relay5->turnOff();
      COLORTEMPERATURE();
      FastLED.show();
      FastLED.delay(1000 / FRAMES_PER_SECOND / 2);
      break;
    case 3:
      virtual_relay1->turnOff();
      virtual_relay2->turnOff();
      virtual_relay3->turnOn();
      virtual_relay4->turnOff();
      virtual_relay5->turnOff();
      EVERY_N_MILLISECONDS(20) {
        Pacifica_loop();
        FastLED.show();
      }
      break;
    case 4:
      virtual_relay1->turnOff();
      virtual_relay2->turnOff();
      virtual_relay3->turnOff();
      virtual_relay4->turnOn();
      virtual_relay5->turnOff();
      Fire2012(); // run simulation frame
      FastLED.show(); // display this frame
      FastLED.delay(1000 / FRAMES_PER_SECOND * 2);
      break;
    case 5:
      virtual_relay1->turnOff();
      virtual_relay2->turnOff();
      virtual_relay3->turnOff();
      virtual_relay4->turnOff();
      virtual_relay5->turnOn();
      Fire2012WithPalette(); // run simulation frame, using palette colors
      FastLED.show(); // display this frame
      FastLED.delay(1000 / FRAMES_PER_SECOND);
      break;
    case 100:
      break;
  }
}

// Klasa obsługująca akcje przycisku ON/OFF
class RGBWToggleHandler : public Supla::ActionHandler {
public:
  void handleAction(int event, int action) override {
    if (r_g_b_w) {
      r_g_b_w->toggle(); // Przełącz LED-y RGBW
    }
  }
};
RGBWToggleHandler *rgbwToggleHandler = new RGBWToggleHandler();


// Klasa obsługująca akcje przycisku przełączającego tryby
class CustomActionHandler : public Supla::ActionHandler {
public:
  void handleAction(int event, int action) override { // Używamy właściwego podpisu metody
    if (action == Supla::TOGGLE) { // Sprawdzamy, czy akcja to przełączanie trybów
      nextMode();
    }
  }
};
CustomActionHandler *customHandler = new CustomActionHandler();

class RgbwLeds : public Supla::Control::RGBWBase {
  public:
    RgbwLeds(int redPin,
             int greenPin,
             int bluePin,
             int colorBrightnessPin,
             int brightnessPin)
      : redPin(redPin),
        greenPin(greenPin),
        bluePin(bluePin),
        colorBrightnessPin(colorBrightnessPin),
        brightnessPin(brightnessPin) {
    }
    void setRGBWValueOnDevice(uint32_t red,
                              uint32_t green,
                              uint32_t blue,
                              uint32_t colorBrightness,
                              uint32_t brightness) override {
      //       analogWrite(brightnessPin, (brightness * 255) / 100);
      //       analogWrite(colorBrightnessPin, (colorBrightness * 255) / 100);
      //       analogWrite(redPin,   red);
      //       analogWrite(greenPin, green);
      //       analogWrite(bluePin,  blue);
      if ( colorBrightnessPin > 5) {
        R_G_B_W = true; 

        // włączenie KOLORU RGB wyłącza pozostałe kanały jeżeli któryś jest włączony //
        if ( virtual_relay1->isOn())  virtual_relay1->turnOff();
        if ( virtual_relay2->isOn())  virtual_relay2->turnOff();
        if ( virtual_relay3->isOn())  virtual_relay3->turnOff();
        if ( virtual_relay4->isOn())  virtual_relay4->turnOff();
        if ( virtual_relay5->isOn())  virtual_relay5->turnOff();

        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].r = red   * colorBrightness / 200 ;   // max red   255 * max colorBrightness 100 / 170 =  150 maksymalna jasność (zasilacz 2A)
          leds[i].g = green * colorBrightness / 200 ;  // max green 255 * max colorBrightness 100 / 170 =  150 maksymalna jasność (zasilacz 2A)
          leds[i].b = blue  * colorBrightness / 200 ; // max blue  255 * max colorBrightness 100 / 170 =  150 maksymalna jasność (zasilacz 2A)
        }
        FastLED.show();
      }
    }
  protected:
    int redPin;
    int greenPin;
    int bluePin;
    int brightnessPin;
    int colorBrightnessPin;
};

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, false); // inverted state
Supla::EspWebServer suplaServer;


// HTML www component (they appear in sections according to creation
// sequence)
Supla::Html::DeviceInfo htmlDeviceInfo(&SuplaDevice);
Supla::Html::WifiParameters htmlWifi;
Supla::Html::ProtocolParameters htmlProto;
Supla::Html::StatusLedParameters htmlStatusLed;

void setup() {
  Serial.begin(115200);
  Supla::Storage::Init();
  // Wczytaj zapisaną nazwę urządzenia
  if (Supla::Storage::ConfigInstance()->getString(DEV_NAME, devName, 30)) {
    SUPLA_LOG_DEBUG("# Param[%s]: %s", DEV_NAME, devName);
  } else {
    SUPLA_LOG_DEBUG("# Param[%s] is not set", DEV_NAME);
  }
  Supla::Storage::Init();
  // Wczytaj zapisany pin LED (Din)
  int32_t savedLedPin = LED_PIN;
  if (Supla::Storage::ConfigInstance()->getInt32(LED_PIN_PARAM, &savedLedPin)) {
      SUPLA_LOG_DEBUG("# Param[%s]: %d", LED_PIN_PARAM, savedLedPin);
  } else {
      SUPLA_LOG_DEBUG("# Param[%s] is not set, using default: %d", LED_PIN_PARAM, LED_PIN);
  }
    Supla::Storage::Init();
  // Wczytaj zapisany CONFIG
  int32_t savedConfigPin = CONFIG_PIN;
  if (Supla::Storage::ConfigInstance()->getInt32(CONFIG_PIN_PARAM, &savedConfigPin)) {
      SUPLA_LOG_DEBUG("# Param[%s]: %d", CONFIG_PIN_PARAM, savedConfigPin);
  } else {
      SUPLA_LOG_DEBUG("# Param[%s] is not set, using default: %d", CONFIG_PIN_PARAM, CONFIG_PIN);
  }
  // Wczytaj zapisaną liczbę LED-ów
  int32_t savedNumLeds = NUM_LEDS;
  if (Supla::Storage::ConfigInstance()->getInt32(NUM_LEDS_PARAM, &savedNumLeds)) {
      SUPLA_LOG_DEBUG("# Param[%s]: %d", NUM_LEDS_PARAM, savedNumLeds);
  } else {
      SUPLA_LOG_DEBUG("# Param[%s] is not set, using default: %d", NUM_LEDS_PARAM, NUM_LEDS);
  }
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, savedNumLeds).setCorrection( TypicalLEDStrip );
      // Dynamiczne przypisanie pinu przy użyciu WS2812B
//    FastLED.addLeds<CHIPSET, COLOR_ORDER>(leds, savedNumLeds, savedLedPin);
//    FastLED.setBrightness( BRIGHTNESS );
    gPal = HeatColors_p;


  WiFi.mode(WIFI_STA);

  /*
     Having your device already registered at cloud.supla.org,
     you want to change CHANNEL sequence or remove any of them,
     then you must also remove the device itself from cloud.supla.org.
     Otherwise you will get "Channel conflict!" error.

     SuplaDevice Initialization.
     Server address is available at https://cloud.supla.org
     If you do not have an account, you can create it at https://cloud.supla.org/account/create
     SUPLA and SUPLA CLOUD are free of charge

  */

  r_g_b_w = new RgbwLeds(RED_PIN, GREEN_PIN, BLUE_PIN, COLOR_BRIGHTNESS_PIN, BRIGHTNESS_PIN); // kanał RGBW
  virtual_relay1 = new Supla::Control::VirtualRelay();                                       // wirtualny kanał // Confetti(
  virtual_relay2 = new Supla::Control::VirtualRelay();                                      // wirtualny kanał // ColorTemperature()
  virtual_relay3 = new Supla::Control::VirtualRelay();                                     // wirtualny kanał // Pacifica()
  virtual_relay4 = new Supla::Control::VirtualRelay();                                    // wirtualny kanał // Fire2012()
  virtual_relay5 = new Supla::Control::VirtualRelay();                                   // wirtualny kanał // Fire2012WithPalette()

  OFF();

  // Dodanie przycisku CONFIG
  auto configButton = new Supla::Control::Button(savedConfigPin, true, true);
  configButton->setHoldTime(1500); // Ustawienie czasu HOLD przycisku
  configButton->addAction(Supla::ENTER_CONFIG_MODE, SuplaDevice, Supla::ON_CLICK_5); // ustawienie trybu CONFIG
  configButton->addAction(Supla::TOGGLE, customHandler, Supla::ON_CLICK_1); // Przypisanie akcji przełączania trybów
  configButton->addAction(Supla::TOGGLE, rgbwToggleHandler, Supla::ON_HOLD); // Przypisanie akcji włączania/wyłączania RGBW

  // Dodanie pola wyboru z niestandardową listą dostępnych pinów
  auto ledPinSelect = new Supla::Html::SelectInputParameter(LED_PIN_PARAM, "LED Pin (domyślnie: IO16)");

  // Wypisanie niestandardowych pinów ręcznie
  ledPinSelect->registerValue("4", 4);
  ledPinSelect->registerValue("15", 15);
  ledPinSelect->registerValue("16", 16);
  ledPinSelect->registerValue("17", 17);
  ledPinSelect->registerValue("18", 18);
  ledPinSelect->registerValue("19", 19);
  ledPinSelect->registerValue("20", 20);
  ledPinSelect->registerValue("21", 21);
  ledPinSelect->registerValue("22", 22);
  ledPinSelect->registerValue("23", 23);
  ledPinSelect->registerValue("24", 24);
  ledPinSelect->registerValue("25", 25);
  ledPinSelect->registerValue("26", 26);
  ledPinSelect->registerValue("27", 27);
  ledPinSelect->registerValue("28", 28);
  ledPinSelect->registerValue("29", 29);
  ledPinSelect->registerValue("30", 30);
  ledPinSelect->registerValue("31", 31);
  ledPinSelect->registerValue("32", 32);
  ledPinSelect->registerValue("33", 33);

  // Dodanie HTML do wyboru CONFIG_PIN
  auto configPinSelect = new Supla::Html::SelectInputParameter(CONFIG_PIN_PARAM, "Pin przycisku (CONFIG) (domyślnie: IO25)");
  configPinSelect->registerValue("0", 0);
  configPinSelect->registerValue("4", 4);
  configPinSelect->registerValue("5", 5);
  configPinSelect->registerValue("15", 15);
  configPinSelect->registerValue("16", 16);
  configPinSelect->registerValue("17", 17);

  // Pole do ustawienia liczby diod LED
  new Supla::Html::CustomParameter(NUM_LEDS_PARAM, "Ilość diod (domyślnie: 7)");

  // nazwa urządzenia
  new Supla::Html::CustomTextParameter(DEV_NAME, "Nazwa urządzenia", 30);
  SuplaDevice.setName(devName);

  // configure defualt Supla CA certificate
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

  // domyślne funkcje i nazwy kanałów
  virtual_relay1->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  virtual_relay1->setInitialCaption("Konfetti");
  virtual_relay2->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  virtual_relay2->setInitialCaption("Color temperature");
  virtual_relay3->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  virtual_relay3->setInitialCaption("Pacifica");
  virtual_relay4->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  virtual_relay4->setInitialCaption("Fire2012");
  virtual_relay5->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  virtual_relay5->setInitialCaption("Fire2012 z paletą");

  SuplaDevice.begin();
}

void loop() {

  if ( virtual_relay1->isOn() && X1 == false) {
    Serial.println("kanał 1 On");
    X1 = true;
    X0 = true;
    // jeżeli włączony 1 kanał to wyłącza pozostałe jeżeli któryś jest włączony //
    if ( virtual_relay2->isOn())  virtual_relay2->turnOff();
    if ( virtual_relay3->isOn())  virtual_relay3->turnOff();
    if ( virtual_relay4->isOn())  virtual_relay4->turnOff();
    if ( virtual_relay5->isOn())  virtual_relay5->turnOff();
    /* // włączenie wirtualnego kanału wyłącza RGBW // niestety użycie tego resetuje esp
        if (  R_G_B_W == true) {
          r_g_b_w->setRGBWValueOnDevice(0, 0, 0, 0, 150);
          // mirgbw->setRGBWValueOnDevice(hw_red, hw_green, hw_blue, hw_colorBrightness, hw_brightness);
          R_G_B_W = false;
        }
    */
    FUNKCJA = 1;  Serial.print(" FUNKCJA: "); Serial.print(FUNKCJA); Serial.println(" Confetti()");
  }
  if ( !virtual_relay1->isOn() && X1 == true) {
    X1 = false;
    Serial.println("kanał 1 Off");
  }

  if ( virtual_relay2->isOn() && X2 == false) {
    Serial.println("kanał 2 On");
    X2 = true;
    X0 = true;
    // jeżeli włączony 2 kanał to wyłącza pozostałe jeżeli któryś jest włączony //
    if ( virtual_relay1->isOn())  virtual_relay1->turnOff(); // jeżeli włączony 2 kanał to wyłącza pozostałe //
    if ( virtual_relay3->isOn())  virtual_relay3->turnOff();
    if ( virtual_relay4->isOn())  virtual_relay4->turnOff();
    if ( virtual_relay5->isOn())  virtual_relay5->turnOff();
    FUNKCJA = 2; Serial.print(" FUNKCJA: "); Serial.print(FUNKCJA); Serial.println(" ColorTemperature()");
    ON();
  }
  if ( !virtual_relay2->isOn() && X2 == true) {
    X2 = false;
    Serial.println("kanał 2 Off");
  }

  if ( virtual_relay3->isOn() && X3 == false) {
    Serial.println("kanał 3 On");
    X3 = true;
    X0 = true;
    // jeżeli włączony 3 kanał to wyłącza pozostałe jeżeli któryś jest włączony //
    if ( virtual_relay1->isOn())  virtual_relay1->turnOff();
    if ( virtual_relay2->isOn())  virtual_relay2->turnOff();
    if ( virtual_relay4->isOn())  virtual_relay4->turnOff();
    if ( virtual_relay5->isOn())  virtual_relay5->turnOff();
    FUNKCJA = 3; Serial.print(" FUNKCJA: "); Serial.print(FUNKCJA); Serial.println(" Pacifica()");
    ON();
  }
  if ( !virtual_relay3->isOn() && X3 == true) {
    X3 = false;
    Serial.println("kanał 3 Off");
  }

  if ( virtual_relay4->isOn() && X4 == false) {
    Serial.println("kanał 4 On");
    X4 = true;
    X0 = true;
    // jeżeli włączony 4 kanał to wyłącza pozostałe jeżeli któryś jest włączony //
    if ( virtual_relay1->isOn())  virtual_relay1->turnOff();
    if ( virtual_relay2->isOn())  virtual_relay2->turnOff();
    if ( virtual_relay3->isOn())  virtual_relay3->turnOff();
    if ( virtual_relay5->isOn())  virtual_relay5->turnOff();
    FUNKCJA = 4; Serial.print(" FUNKCJA: "); Serial.print(FUNKCJA);  Serial.println(" Fire2012()");
  }
  if ( !virtual_relay4->isOn() && X4 == true) {
    X4 = false;
    Serial.println("kanał 4 Off");
  }

  if ( virtual_relay5->isOn() && X5 == false) {
    Serial.println("kanał 5 On");
    X5 = true;
    X0 = true;
    // jeżeli włączony 5 kanał to wyłącza pozostałe jeżeli któryś jest włączony //
    if ( virtual_relay1->isOn())  virtual_relay1->turnOff();
    if ( virtual_relay2->isOn())  virtual_relay2->turnOff();
    if ( virtual_relay3->isOn())  virtual_relay3->turnOff();
    if ( virtual_relay4->isOn())  virtual_relay4->turnOff();
    FUNKCJA = 5; Serial.print(" FUNKCJA: "); Serial.print(FUNKCJA); Serial.println(" Fire2012WithPalette()");
  }
  if ( !virtual_relay5->isOn() && X5 == true) {
    X5 = false;
    Serial.println("kanał 5 Off");
  }

  if ( !virtual_relay1->isOn() && !virtual_relay2->isOn() && !virtual_relay3->isOn() && !virtual_relay4->isOn() && !virtual_relay5->isOn() && X0 == true) {
    Serial.println("kanał ALL Off");
    X0 = false;
    FUNKCJA = 0;
    Serial.println("lampa OFF");
  }

  SuplaDevice.iterate();


}



void ON() {
  for ( int i = 0; i < 100; i++ ) {
    long Number0 = random(0, NUM_LEDS);
    long Number1 = random(0, 100);
    long Number2 = random(0, 100);
    long Number3 = random(0, 100);
    leds[Number0] = CHSV(Number1, Number2, Number3);
    FastLED.show();
    delay(1);
  }
}

// wyłączenie lampy//
void OFF() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[NUM_LEDS - i - 1] = CHSV(0, 0, 0);
    FastLED.show();
    delay(10);
  }
}

#define COOLING  55
#define SPARKING 120
void Fire2012()
{
  // Array of temperature readings at each simulation cell
  static uint8_t heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    CRGB color = HeatColor( heat[j]);
    int pixelnumber;
    if ( gReverseDirection ) {
      pixelnumber = (NUM_LEDS - 1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
}


void Fire2012WithPalette()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    byte colorindex = scale8( heat[j], 240);
    CRGB color = ColorFromPalette( gPal, colorindex);
    int pixelnumber;
    if ( gReverseDirection ) {
      pixelnumber = (NUM_LEDS - 1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
}

void COLORTEMPERATURE() {
  // draw a generic, no-name rainbow
  static uint8_t starthue = 0;
  fill_rainbow( leds + 5, NUM_LEDS - 5, --starthue, 20);

  // Choose which 'color temperature' profile to enable.
  uint8_t secs = (millis() / 1000) % (DISPLAYTIME);
  if ( secs < DISPLAYTIME) {
    FastLED.setTemperature( TEMPERATURE_1 ); // first temperature
    leds[0] = TEMPERATURE_1; // show indicator pixel
  } else {
    FastLED.setTemperature( TEMPERATURE_2 ); // second temperature
    leds[0] = TEMPERATURE_2; // show indicator pixel
  }
}

void Confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}


CRGBPalette16 pacifica_palette_1 =
{ 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
  0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50
};
CRGBPalette16 pacifica_palette_2 =
{ 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
  0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F
};
CRGBPalette16 pacifica_palette_3 =
{ 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
  0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF
};


void Pacifica_loop()
{
  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011, 10, 13));
  sCIStart2 -= (deltams21 * beatsin88(777, 8, 11));
  sCIStart3 -= (deltams1 * beatsin88(501, 5, 7));
  sCIStart4 -= (deltams2 * beatsin88(257, 4, 6));

  // Clear out the LED array to a dim background blue-green
  fill_solid( leds, NUM_LEDS, CRGB( 2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary over time
  pacifica_one_layer( pacifica_palette_1, sCIStart1, beatsin16( 3, 11 * 256, 14 * 256), beatsin8( 10, 70, 130), 0 - beat16( 301) );
  pacifica_one_layer( pacifica_palette_2, sCIStart2, beatsin16( 4,  6 * 256,  9 * 256), beatsin8( 17, 40,  80), beat16( 401) );
  pacifica_one_layer( pacifica_palette_3, sCIStart3, 6 * 256, beatsin8( 9, 10, 38), 0 - beat16(503));
  pacifica_one_layer( pacifica_palette_3, sCIStart4, 5 * 256, beatsin8( 8, 10, 28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps();

  // Deepen the blues and greens a bit
  pacifica_deepen_colors();
}

// Add one layer of waves into the led array
void pacifica_one_layer( CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    waveangle += 250;
    uint16_t s16 = sin16( waveangle ) + 32768;
    uint16_t cs = scale16( s16 , wavescale_half ) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16( ci) + 32768;
    uint8_t sindex8 = scale16( sindex16, 240);
    CRGB c = ColorFromPalette( p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps()
{
  uint8_t basethreshold = beatsin8( 9, 55, 65);
  uint8_t wave = beat8( 7 );

  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t threshold = scale8( sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();
    if ( l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8( overage, overage);
      leds[i] += CRGB( overage, overage2, qadd8( overage2, overage2));
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors()
{
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].blue = scale8( leds[i].blue,  145);
    leds[i].green = scale8( leds[i].green, 200);
    leds[i] |= CRGB( 2, 5, 7);
  }
}