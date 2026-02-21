# CustomEyeEngine for ESP32

A lightweight, procedural eye animation library for SSD1306 OLED displays. Designed to replace third-party libraries with custom, smooth, and memory-efficient animations.

## Features
- **Procedural Drawing**: No bitmaps, extremely low memory footprint.
- **Smooth Transitions**: Uses Linear Interpolation (Lerp) for fluid mood changes.
- **1000+ Expressions**: Combinatorial system using extended `EyeParameters` (iris, reflections, eyelid curves, eyebrows).
- **Interactive**: Support for blinking, gaze control (`lookAt`), and macro animations (Confused, Laughing, Dizzy).
- **Special Overlays**: Heart eyes, Star eyes, Cross eyes, Scanning effects, and more.
- **Interface Modes**: Built-in support for Battery, Weather, and Bluetooth status icons.

## Moods List
`DEFAULT`, `HAPPY`, `ANGRY`, `TIRED`, `CURIOUS`, `ANXIOUS`, `PLAYFUL`, `SLEEPY`, `SAD`, `SURPRISED`, `CONFIDENT`, `SNEAKY`, `QUESTIONING`, `DIZZY`, `SCARED`, `HEART_EYES`, `STAR_EYES`, `ZOMBIE`, `FIREWORKS`, `BATTERY_LOW`, `CHARGING`, `WEATHER_SUN`, `WEATHER_RAIN`, `WEATHER_SNOW`, `BLUETOOTH_CONN`

## Installation
1. Download this repository as a ZIP.
2. In Arduino IDE, go to **Sketch > Include Library > Add .ZIP Library**.
3. Ensure you have the **Adafruit SSD1306** and **Adafruit GFX** libraries installed.

## Usage
```cpp
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <CustomEyeEngine.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
CustomEyeEngine roboEyes(&display);

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  roboEyes.begin();
  roboEyes.setMood(HAPPY);
}

void loop() {
  roboEyes.update();
}
```
