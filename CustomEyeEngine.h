#ifndef CUSTOM_EYE_ENGINE_H
#define CUSTOM_EYE_ENGINE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

enum Mood { DEFAULT, HAPPY, ANGRY, TIRED, CURIOUS, ANXIOUS, PLAYFUL, SLEEPY };

struct EyeParameters {
  int centerX;      
  int centerY;      
  int width;        
  int height;       
  int borderRadius; // Added border radius for rounded rect eyes
  int squintTop;    
  int squintBottom; 
  int pupilSize;    
  int pupilOffsetX; 
  int pupilOffsetY; 
  bool isAngry;     // Special flag for triangle eyelid overlay
  bool isTired;     // Special flag for triangle eyelid overlay
};

struct EyeAnimationState {
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
  
  // Macro Animations (Inspired by RoboEyes)
  void setAutoblinker(bool active, int intervalSec = 3, int variationSec = 2);
  void setIdleMode(bool active, int intervalSec = 2, int variationSec = 2);
  void setConfused(bool active, int durationMs = 500);
  void setLaughing(bool active, int durationMs = 500);

private:
  Adafruit_SSD1306* _display;
  EyeAnimationState _leftEyeState;
  EyeAnimationState _rightEyeState;
  int _screenWidth;
  int _screenHeight;

  // Autoblinker & Idle Timers
  bool _autoblinker = false;
  int _blinkInterval, _blinkVariation;
  unsigned long _nextBlinkTime = 0;

  bool _idleMode = false;
  int _idleInterval, _idleVariation;
  unsigned long _nextIdleTime = 0;

  // Shiver/Flicker effects
  bool _confused = false;
  unsigned long _confusedTimer = 0;
  int _confusedDuration = 500;

  bool _laughing = false;
  unsigned long _laughingTimer = 0;
  int _laughingDuration = 500;

  EyeParameters getParamsForMood(Mood mood);
  void drawEye(const EyeParameters& params, bool isLeftEye);
  void drawEyelid(const EyeParameters& params, bool isLeftEye);
  void drawPupil(int centerX, int centerY, int size, int offsetX, int offsetY);
  void interpolateEyeParams(EyeAnimationState& state);
  int lerp(int start, int end, float progress);
};

#endif
