#ifndef WLED_PIN_MANAGER_H
#define WLED_PIN_MANAGER_H

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP32
#include "driver/ledc.h" // needed for analog/LEDC channel counts
#endif

typedef struct PinManagerPinType {
  int8_t pin;
  bool   isOutput;
} managed_pin_type;

enum struct PinOwner : uint8_t {
  None          = 0,      // default == legacy == unspecified owner
  BusDigital    = 0x82,   // 'BusD' == Digital output using BusDigital
};

class PinManager {
public:
  static bool allocatePin(byte gpio, bool output, PinOwner tag);
  static bool deallocatePin(byte gpio, PinOwner tag);
  static bool isPinAllocated(byte gpio, PinOwner tag = PinOwner::None);
  static bool isPinOk(byte gpio, bool output);

private:
  static uint64_t pinAlloc;
  static PinOwner ownerTag[40]; // Adjust size as needed
};

#endif // WLED_PIN_MANAGER_H