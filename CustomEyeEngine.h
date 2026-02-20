#ifndef CUSTOM_EYE_ENGINE_H
#define CUSTOM_EYE_ENGINE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

enum Mood { DEFAULT, HAPPY, ANGRY, TIRED, CURIOUS, ANXIOUS, PLAYFUL };

struct EyeParameters {
  int centerX;      
  int centerY;      
  int width;        
  int height;       
  int squintTop;    
  int squintBottom; 
  int pupilSize;    
  int pupilOffsetX; 
  int pupilOffsetY; 
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
  void setMood(Mood newMood, unsigned long duration = 500);
  void setCustomParams(const EyeParameters& params, unsigned long animDuration = 200);
  void blink(unsigned long duration = 150);
  void lookAt(int x, int y, unsigned long duration = 150);

private:
  Adafruit_SSD1306* _display;
  EyeAnimationState _leftEyeState;
  EyeAnimationState _rightEyeState;
  int _screenWidth;
  int _screenHeight;

  EyeParameters getParamsForMood(Mood mood);
  void drawEye(const EyeParameters& params, bool isLeftEye);
  void drawEyelid(int centerX, int centerY, int width, int height, int offset, bool isTop);
  void drawPupil(int centerX, int centerY, int size, int offsetX, int offsetY);
  void interpolateEyeParams(EyeAnimationState& state);
  int lerp(int start, int end, float progress);
};

#endif
