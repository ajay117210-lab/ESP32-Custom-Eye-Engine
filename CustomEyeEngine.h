#ifndef CUSTOM_EYE_ENGINE_H
#define CUSTOM_EYE_ENGINE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =============================================================================
// CustomEyeEngine v2.0
// A procedural, expressive eye animation library for SSD1306 OLED displays.
// Designed for ESP32 / Arduino-based robotic faces.
// =============================================================================

// --- Mood Types --------------------------------------------------------------
enum Mood {
  NEUTRAL, HAPPY, ANGRY, TIRED, CURIOUS, ANXIOUS, PLAYFUL, SLEEPY,
  SAD, SURPRISED, CONFIDENT, SNEAKY, QUESTIONING, DIZZY, SCARED,
  HEART_EYES, STAR_EYES, ZOMBIE, FIREWORKS, BATTERY_LOW, CHARGING,
  WEATHER_SUN, WEATHER_RAIN, WEATHER_SNOW, BLUETOOTH_CONN
};

// --- Gaze Direction (cardinal + default) ------------------------------------
enum GazeDirection {
  GAZE_DEFAULT,
  GAZE_N, GAZE_NE, GAZE_E, GAZE_SE,
  GAZE_S, GAZE_SW, GAZE_W, GAZE_NW
};

// --- Eye Shape Parameters ----------------------------------------------------
struct EyeParameters {
  // Position & Shape
  int centerX;
  int centerY;
  int width;
  int height;
  int borderRadius;

  // Eyelids (squint)
  int squintTop;
  int squintBottom;

  // Pupil
  int pupilSize;
  int pupilOffsetX;
  int pupilOffsetY;

  // Iris (ring around pupil)
  int irisSize;
  int irisOffsetX;
  int irisOffsetY;

  // Glint / Reflection
  int reflectionSize;
  int reflectionOffsetX;
  int reflectionOffsetY;

  // Eyelid curve: 0=flat, >0=happy curve bottom, <0=sad curve bottom
  int eyelidCurve;

  // Eyebrow
  int eyebrowAngle;   // degrees, positive tilts inward (angry), negative outward
  int eyebrowHeight;  // distance above eye top edge (0 = hidden)
  int eyebrowThick;   // thickness in pixels

  // Boolean overlays
  bool isAngry;
  bool isTired;
  bool hasHeart;
  bool hasStar;
  bool hasCross;      // Dizzy X eyes
  bool isScanning;    // Horizontal scanning line
};

// --- Per-eye animation state -------------------------------------------------
struct EyeAnimationState {
  EyeParameters currentParams;
  EyeParameters targetParams;
  unsigned long startTime;
  unsigned long duration;
  bool animating;
  bool isOpen;        // Tracks open/closed for blink cycling
};

// --- Sweat drop state --------------------------------------------------------
struct SweatDrop {
  bool active;
  int x, y;          // current position
  int startY;        // reset point
  unsigned long lastUpdate;
};

// =============================================================================
// CustomEyeEngine Class
// =============================================================================
class CustomEyeEngine {
public:
  // Constructor: pass a pointer to your Adafruit_SSD1306 display
  CustomEyeEngine(Adafruit_SSD1306* displayPtr);

  // Core lifecycle
  void begin();
  void update();  // Call this every loop() iteration

  // --- Mood & Expression Control ---
  void setMood(Mood newMood, unsigned long animDuration = 500);
  void setCustomParams(const EyeParameters& params, unsigned long animDuration = 200);

  // --- Gaze Control ---
  void lookAt(int x, int y, unsigned long duration = 200);
  void lookAt(GazeDirection direction, unsigned long duration = 200);

  // --- Blink & Wink ---
  void blink(unsigned long closeDuration = 80, unsigned long openDuration = 80);
  void wink(bool leftEye, unsigned long closeDuration = 120, unsigned long openDuration = 120);

  // --- Macro Animations ---
  void setAutoblinker(bool active, float intervalSec = 3.0f, float variationSec = 2.0f);
  void setIdleMode(bool active, float intervalSec = 2.5f, float variationSec = 2.0f);
  void setCuriosityMode(bool active);  // Eyes widen when looking far left/right
  void setConfused(bool active, int durationMs = 600);
  void setLaughing(bool active, int durationMs = 600);
  void setDizzy(bool active, int durationMs = 1200);
  void setSweat(bool active);          // Animated sweat drops

  // --- Framerate Cap ---
  void setMaxFPS(int fps);             // Default: 60

private:
  Adafruit_SSD1306* _display;
  EyeAnimationState _leftEyeState;
  EyeAnimationState _rightEyeState;
  int _screenWidth;
  int _screenHeight;

  // Framerate limiter
  int _maxFPS = 60;
  unsigned long _lastFrameTime = 0;

  // Autoblinker
  bool _autoblinker = false;
  float _blinkInterval = 3.0f;
  float _blinkVariation = 2.0f;
  unsigned long _nextBlinkTime = 0;
  bool _blinkInProgress = false;
  bool _blinkReopening = false;
  unsigned long _blinkReopenTime = 0;
  unsigned long _blinkOpenDuration = 80;

  // Idle / wander
  bool _idleMode = false;
  float _idleInterval = 2.5f;
  float _idleVariation = 2.0f;
  unsigned long _nextIdleTime = 0;

  // Curiosity mode
  bool _curiosityMode = false;

  // Wink state
  bool _winkInProgress = false;
  bool _winkLeft = false;
  unsigned long _winkReopenTime = 0;
  unsigned long _winkOpenDuration = 120;

  // Macro effects
  bool _confused = false;
  unsigned long _confusedTimer = 0;
  unsigned long _confusedDuration = 600;

  bool _laughing = false;
  unsigned long _laughingTimer = 0;
  unsigned long _laughingDuration = 600;

  bool _dizzy = false;
  unsigned long _dizzyTimer = 0;
  unsigned long _dizzyDuration = 1200;

  // Sweat drop
  bool _sweat = false;
  SweatDrop _sweatDrop;

  // Internal helpers
  EyeParameters getParamsForMood(Mood mood);
  void drawEye(const EyeParameters& params, bool isLeftEye);
  void drawIris(int cx, int cy, int size, int ox, int oy);
  void drawPupil(int cx, int cy, int size, int ox, int oy);
  void drawReflection(int cx, int cy, int size, int ox, int oy);
  void drawEyelid(const EyeParameters& params, bool isLeftEye);
  void drawEyebrow(const EyeParameters& params, bool isLeftEye);
  void drawSpecialOverlays(const EyeParameters& params, bool isLeftEye);
  void drawSweatDrop(unsigned long currentTime);

  void interpolateEyeParams(EyeAnimationState& state, unsigned long currentTime);
  void applyBlinkToEye(EyeAnimationState& state, unsigned long closeDuration);
  void reopenEye(EyeAnimationState& state, unsigned long openDuration);
  void applyCuriosity(EyeParameters& l, EyeParameters& r);

  int lerp(int a, int b, float t);
  float easeInOut(float t);
};

#endif // CUSTOM_EYE_ENGINE_H
