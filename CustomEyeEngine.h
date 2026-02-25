#ifndef CUSTOM_EYE_ENGINE_H
#define CUSTOM_EYE_ENGINE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Expanded Mood Types to cover more categories
enum Mood { 
  NEUTRAL, HAPPY, ANGRY, TIRED, CURIOUS, ANXIOUS, PLAYFUL, SLEEPY, 
  SAD, SURPRISED, CONFIDENT, SNEAKY, QUESTIONING, DIZZY, SCARED, 
  HEART_EYES, STAR_EYES, ZOMBIE, FIREWORKS, BATTERY_LOW, CHARGING,
  WEATHER_SUN, WEATHER_RAIN, WEATHER_SNOW, BLUETOOTH_CONN
};

struct EyeParameters {
  int centerX;      
  int centerY;      
  int width;        
  int height;       
  int borderRadius; 
  int squintTop;    
  int squintBottom; 
  int pupilSize;    
  int pupilOffsetX; 
  int pupilOffsetY; 
  
  // New Parameters for more expressiveness
  int irisSize;         
  int irisOffsetX;      
  int irisOffsetY;      
  int reflectionSize;   
  int reflectionOffsetX;
  int reflectionOffsetY;
  int eyelidCurve;      // 0 = flat, positive = happy curve, negative = sad curve
  int eyebrowAngle;     // Angle in degrees
  int eyebrowHeight;    // Offset from eye top
  
  // Boolean flags for special overlays
  bool isAngry;     
  bool isTired;     
  bool hasHeart;
  bool hasStar;
  bool hasCross;    // For dizzy/dead eyes
  bool isScanning;  // For a scanning line effect
};

struct EyeAnimationState {
  EyeParameters startParams;
  EyeParameters currentParams; 
  EyeParameters targetParams;  
  unsigned long startTime;     
  unsigned long duration;      
  bool animating;              
};

class CustomEyeEngine {
public:
  CustomEyeEngine(Adafruit_SSD1306* displayPtr);
  void begin();
  void update();
  
  // Mood and Parameter Control
  void setMood(Mood newMood, unsigned long duration = 500);
  void setCustomParams(const EyeParameters& params, unsigned long animDuration = 200);
  
  // Animation Triggers
  void blink(unsigned long duration = 150);
  void lookAt(int x, int y, unsigned long duration = 150);
  
  // Macro Animations
  void setAutoblinker(bool active, int intervalSec = 3, int variationSec = 2);
  void setIdleMode(bool active, int intervalSec = 2, int variationSec = 2);
  void setConfused(bool active, int durationMs = 500);
  void setLaughing(bool active, int durationMs = 500);
  void setDizzy(bool active, int durationMs = 1000);

private:
  Adafruit_SSD1306* _display;
  EyeAnimationState _leftEyeState;
  EyeAnimationState _rightEyeState;
  int _screenWidth;
  int _screenHeight;
  EyeParameters _baseMoodParams;
  int _gazeOffsetX = 0;
  int _gazeOffsetY = 0;

  bool _blinkActive = false;
  bool _blinkClosing = false;
  unsigned long _blinkPhaseStart = 0;
  unsigned long _blinkDuration = 150;

  // Autoblinker & Idle Timers
  bool _autoblinker = false;
  int _blinkInterval, _blinkVariation;
  unsigned long _nextBlinkTime = 0;

  bool _idleMode = false;
  int _idleInterval, _idleVariation;
  unsigned long _nextIdleTime = 0;

  // Effects
  bool _confused = false;
  unsigned long _confusedTimer = 0;
  int _confusedDuration = 500;

  bool _laughing = false;
  unsigned long _laughingTimer = 0;
  int _laughingDuration = 500;

  bool _dizzy = false;
  unsigned long _dizzyTimer = 0;
  int _dizzyDuration = 1000;

  EyeParameters getParamsForMood(Mood mood);
  void drawEye(const EyeParameters& params, bool isLeftEye);
  void drawEyelid(const EyeParameters& params, bool isLeftEye);
  void drawPupil(int centerX, int centerY, int size, int offsetX, int offsetY);
  void drawIris(int centerX, int centerY, int size, int offsetX, int offsetY);
  void drawReflection(int centerX, int centerY, int size, int offsetX, int offsetY);
  void drawEyebrow(const EyeParameters& params, bool isLeftEye);
  void drawSpecialOverlays(const EyeParameters& params, bool isLeftEye);

  void startAnimation(EyeAnimationState& state, const EyeParameters& target, unsigned long animDuration);
  bool isAtTarget(const EyeAnimationState& state) const;
  int clampInt(int value, int minValue, int maxValue) const;
  
  void interpolateEyeParams(EyeAnimationState& state);
  int lerp(int start, int end, float progress);
};

#endif
