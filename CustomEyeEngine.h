/*
 * CustomEyeEngine v6.0
 * "Give life to yourself."
 *
 * A parameter-driven robotic facial expression system for SSD1306 OLED displays.
 * Every expression, emotion, and life-like behavior emerges from continuously
 * interpolating a small set of float parameters — never from fixed animations.
 *
 * Architecture:
 *   FaceParameters     — the floating-point "soul" of the face
 *   EmotionEngine      — maps emotions to target parameters
 *   BlinkEngine        — independent, semi-random, multi-variant blinking
 *   MicroEngine        — fires subtle random life-behaviors every 5-10s
 *   BreathingEngine    — continuous sine-wave micro-oscillation
 *   EyebrowEngine      — thin minimal bars with angle + height per eye
 *
 * FluxGarage API compatibility:
 *   begin(), update(), setMood(), setAutoblinker(), setIdleMode(),
 *   setCuriosity(), setPosition(), open(), close(), blink(),
 *   anim_confused(), anim_laugh(), setSweat(), setHFlicker(), setVFlicker()
 *   — all preserved. New expressive API layered on top.
 *
 * Drop-in:
 *   Replace  #include <FluxGarage_RoboEyes.h>
 *   with     #include "CustomEyeEngine.h"
 *   Change   RoboEyes<Adafruit_SSD1306>  →  CustomEyes<Adafruit_SSD1306>
 */

#ifndef CUSTOM_EYE_ENGINE_H
#define CUSTOM_EYE_ENGINE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>

// ── Colour tokens ────────────────────────────────────────────────────────────
uint8_t BGCOLOR   = 0;
uint8_t MAINCOLOR = 1;

// ── Emotion IDs ──────────────────────────────────────────────────────────────
#define EMOTION_NEUTRAL    0
#define EMOTION_HAPPY      1
#define EMOTION_EXCITED    2
#define EMOTION_CURIOUS    3
#define EMOTION_THINKING   4
#define EMOTION_SAD        5
#define EMOTION_SLEEPY     6
#define EMOTION_ANGRY      7
#define EMOTION_SURPRISED  8
#define EMOTION_SUSPICIOUS 9

// ── FluxGarage mood aliases (backward compat) ────────────────────────────────
#define DEFAULT    EMOTION_NEUTRAL
#define TIRED      EMOTION_SLEEPY
#define ANGRY      EMOTION_ANGRY
#define HAPPY      EMOTION_HAPPY
#define SURPRISED  EMOTION_SURPRISED

// Extended aliases
#define CURIOUS    EMOTION_CURIOUS
#define SAD        EMOTION_SAD
#define EXCITED    EMOTION_EXCITED
#define THINKING   EMOTION_THINKING
#define SUSPICIOUS EMOTION_SUSPICIOUS
#define SKEPTICAL  EMOTION_SUSPICIOUS

// ── ON / OFF ─────────────────────────────────────────────────────────────────
#define ON  1
#define OFF 0

// ── Cardinal positions ───────────────────────────────────────────────────────
#define POS_N  1
#define POS_NE 2
#define POS_E  3
#define POS_SE 4
#define POS_S  5
#define POS_SW 6
#define POS_W  7
#define POS_NW 8

#ifndef NE
  #define NE POS_NE
#endif
#ifndef SE
  #define SE POS_SE
#endif
#ifndef SW
  #define SW POS_SW
#endif
#ifndef NW
  #define NW POS_NW
#endif

// ════════════════════════════════════════════════════════════════════════════
//  FaceParameters — the complete floating-point state of the face
//  Everything visible on screen is derived from these numbers.
// ════════════════════════════════════════════════════════════════════════════
struct FaceParameters {
  // Eye shape
  float eyeWidth;        // pixels, both eyes
  float eyeHeight;       // pixels, both eyes
  float cornerRadius;    // border radius, both eyes
  float spacing;         // gap between the two eyes

  // Eyelids (0.0 = fully open, 1.0 = fully closed)
  float lidTopL;         // top eyelid, left eye
  float lidTopR;         // top eyelid, right eye
  float lidBottomL;      // bottom eyelid, left eye
  float lidBottomR;      // bottom eyelid, right eye

  // Tilt simulation (clips top corners asymmetrically)
  // Positive = tilts inward (angry/sad), Negative = tilts outward
  float tiltL;           // -1.0 to +1.0
  float tiltR;           // -1.0 to +1.0

  // Eyebrows (thin horizontal bars above eyes)
  float browHeightL;     // pixels above eye top (0 = hidden)
  float browHeightR;
  float browAngleL;      // tilt: positive = outer end up, negative = outer end down
  float browAngleR;

  // Vertical position offset (sad drifts down, surprised lifts slightly)
  float yOffset;

  // Eye size asymmetry (suspicious: one eye smaller)
  float eyeHeightR_scale; // multiplier on right eye height (1.0 = normal)
  float eyeWidthR_scale;  // multiplier on right eye width

  // Animation energy (controls micro-movement amplitude)
  float motionEnergy;    // 0.0 = frozen, 1.0 = very active

  // Emotion intensity (0.0 to 1.0, scales all mood effects)
  float intensity;
};

// ════════════════════════════════════════════════════════════════════════════
//  Interpolation helpers
// ════════════════════════════════════════════════════════════════════════════

// Smooth Zeno-style lerp: moves 'frac' of the remaining distance each frame
// frac=0.15 → slow gentle, frac=0.35 → quick snappy
static inline float zlerpf(float cur, float target, float frac) {
  float d = target - cur;
  if (fabsf(d) < 0.01f) return target;
  return cur + d * frac;
}

// Hard snap if very close
static inline float snapf(float cur, float target, float threshold = 0.5f) {
  return (fabsf(cur - target) < threshold) ? target : cur;
}

// ════════════════════════════════════════════════════════════════════════════
//  CustomEyes  — the main class
// ════════════════════════════════════════════════════════════════════════════
template<typename AdafruitDisplay>
class CustomEyes {
public:

  AdafruitDisplay *display;

  // ── Screen config ─────────────────────────────────────────────────────────
  int screenWidth  = 128;
  int screenHeight = 64;
  int frameInterval = 16;   // ~60fps default
  unsigned long fpsTimer = 0;

  // ── Face geometry defaults ────────────────────────────────────────────────
  float defaultWidth    = 36.0f;
  float defaultHeight   = 36.0f;
  float defaultRadius   = 8.0f;
  float defaultSpacing  = 10.0f;

  // ── Current and target FaceParameters ────────────────────────────────────
  FaceParameters cur;   // what is drawn right now
  FaceParameters tgt;   // what we are interpolating toward

  // Lerp speed per parameter group (tuned for personality feel)
  float lerpShape    = 0.18f;   // width, height, radius
  float lerpLid      = 0.20f;   // eyelids
  float lerpTilt     = 0.16f;   // tilt
  float lerpBrow     = 0.14f;   // eyebrows
  float lerpPos      = 0.12f;   // position / spacing
  float lerpAsym     = 0.18f;   // asymmetry

  // ── Current emotion ───────────────────────────────────────────────────────
  int   currentEmotion  = EMOTION_NEUTRAL;
  float currentIntensity = 1.0f;

  // ── Eye absolute positions (screen coords of top-left corner) ────────────
  float eyeLx = 0, eyeLy = 0;
  float eyeRx = 0, eyeRy = 0;
  float eyeLxTarget = 0, eyeLyTarget = 0;   // for setPosition / idle

  // ── Blink engine ──────────────────────────────────────────────────────────
  // Blink is applied as an override on top of the eyelid parameters.
  // blinkT: 0.0 = eyes open, 1.0 = eyes fully closed
  float blinkT           = 0.0f;
  float blinkTTarget     = 0.0f;
  bool  blinkClosing     = false;
  bool  blinkOpening     = false;
  unsigned long blinkTimer = 0;
  unsigned long nextBlink  = 0;

  // Blink variant: 0=normal, 1=slow, 2=double, 3=sleepy
  int   blinkVariant     = 0;
  float blinkCloseSpeed  = 0.45f;   // fraction per frame when closing
  float blinkOpenSpeed   = 0.35f;   // fraction per frame when opening
  bool  doubleBlink2nd   = false;   // second close of double blink pending
  unsigned long doubleBlink2ndAt = 0;

  bool  autoblinker      = true;
  int   blinkInterval    = 3;
  int   blinkIntervalVariation = 3;

  // FluxGarage open/close flags
  bool eyeL_open = true;
  bool eyeR_open = true;

  // ── Micro-expression engine ───────────────────────────────────────────────
  unsigned long nextMicro   = 0;
  int  lastMicro            = -1;   // avoid repeating same action twice
  // Temporary micro-overrides applied to tgt for a short duration
  float microWidthBoost     = 0.0f;
  float microHeightBoost    = 0.0f;
  float microTiltBoost      = 0.0f;
  float microSpacingBoost   = 0.0f;
  unsigned long microEndTime = 0;

  // ── Breathing engine ──────────────────────────────────────────────────────
  float breathPhase         = 0.0f;
  float breathSpeed         = 0.018f;  // ~10-15s period at 60fps
  float breathAmp           = 0.8f;    // ±0.8px

  // ── Excited / Happy bounce ────────────────────────────────────────────────
  float bouncePhase         = 0.0f;
  float bounceAmp           = 0.0f;   // set by emotion engine
  float bounceSpeed         = 0.0f;

  // ── Sad slow drift ────────────────────────────────────────────────────────
  float sadTremblePhase     = 0.0f;
  float sadTrembleAmp       = 0.0f;

  // ── Surprised spring ──────────────────────────────────────────────────────
  bool  surprisedSpringFired = false;
  float surprisedOvershoot   = 0.0f;

  // ── LOVING heart ──────────────────────────────────────────────────────────
  bool  lovingActive        = false;
  float heartMerge          = 0.0f;
  float heartbeatPhase      = 0.0f;

  // ── FluxGarage macro state ────────────────────────────────────────────────
  bool idle          = false;
  int  idleInterval  = 2, idleIntervalVariation = 3;
  unsigned long idleTimer = 0;

  bool curious_mode  = false;   // FluxGarage setCuriosity flag

  bool confused      = false;
  unsigned long confusedTimer = 0;
  bool confusedToggle = true;

  bool laugh         = false;
  unsigned long laughTimer = 0;
  bool laughToggle   = true;

  bool hFlicker      = false;
  bool hFlickerAlt   = false;
  byte hFlickerAmp   = 2;

  bool vFlicker      = false;
  bool vFlickerAlt   = false;
  byte vFlickerAmp   = 10;

  bool sweat         = false;
  byte sweatRadius   = 3;
  int   s1xi=2; int s1x; float s1y=2; int s1ymax; float s1h=2; float s1w=1;
  int   s2xi=2; int s2x; float s2y=2; int s2ymax; float s2h=2; float s2w=1;
  int   s3xi=2; int s3x; float s3y=2; int s3ymax; float s3h=2; float s3w=1;

  // ── anim_shake ────────────────────────────────────────────────────────────
  int  shakePhase    = 0;
  unsigned long shakeTimer = 0;
  const uint8_t ORBIT[8] = {POS_N,POS_NE,POS_E,POS_SE,POS_S,POS_SW,POS_W,POS_NW};
  int  orbitStep=0, orbitCount=0;
  unsigned long orbitTimer=0;
  int  dazedBlinks=0;
  unsigned long dazedTimer=0;

  // ══════════════════════════════════════════════════════════════════════════
  //  Constructor
  // ══════════════════════════════════════════════════════════════════════════
  CustomEyes(AdafruitDisplay &disp) : display(&disp) {}

  // ══════════════════════════════════════════════════════════════════════════
  //  begin()
  // ══════════════════════════════════════════════════════════════════════════
  void begin(int w, int h, byte fps) {
    screenWidth  = w;
    screenHeight = h;
    frameInterval = 1000 / fps;

    // Compute default eye positions (centred)
    float totalW = defaultWidth * 2 + defaultSpacing;
    eyeLx = (screenWidth - totalW) / 2.0f;
    eyeLy = (screenHeight - defaultHeight) / 2.0f;
    eyeRx = eyeLx + defaultWidth + defaultSpacing;
    eyeRy = eyeLy;
    eyeLxTarget = eyeLx; eyeLyTarget = eyeLy;

    // Initialise both param sets to neutral
    _neutralParams(cur);
    _neutralParams(tgt);
    cur.lidTopL = cur.lidTopR = 1.0f;  // start with eyes closed (open on first frame)

    nextBlink = millis() + 1500;
    nextMicro = millis() + random(5000, 10000);

    display->clearDisplay();
    display->display();
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  update() — call every loop()
  // ══════════════════════════════════════════════════════════════════════════
  void update() {
    if (millis() - fpsTimer < (unsigned long)frameInterval) return;
    fpsTimer = millis();
    drawEyes();
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  setEmotion() — the new primary API
  //  intensity: 0.0 to 1.0
  // ══════════════════════════════════════════════════════════════════════════
  void setEmotion(int emotion, float intensity = 1.0f) {
    intensity = constrain(intensity, 0.0f, 1.0f);
    currentEmotion   = emotion;
    currentIntensity = intensity;
    surprisedSpringFired = false;
    lovingActive = (emotion == EMOTION_HAPPY && intensity > 0.9f) ? false : false;
    _buildTarget(emotion, intensity);
  }

  // ── FluxGarage setMood compatibility ─────────────────────────────────────
  void setMood(unsigned char mood) {
    setEmotion((int)mood, 1.0f);
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  FluxGarage setters (full compatibility)
  // ══════════════════════════════════════════════════════════════════════════
  void setFramerate(byte fps)  { frameInterval = 1000/fps; }
  void setDisplayColors(uint8_t bg, uint8_t main) { BGCOLOR=bg; MAINCOLOR=main; }

  void setWidth(byte l, byte r)        { defaultWidth=(l+r)/2.0f; tgt.eyeWidth=defaultWidth; }
  void setHeight(byte l, byte r)       { defaultHeight=(l+r)/2.0f; tgt.eyeHeight=defaultHeight; }
  void setBorderradius(byte l, byte r) { defaultRadius=(l+r)/2.0f; tgt.cornerRadius=defaultRadius; }
  void setSpacebetween(int s)          { defaultSpacing=(float)s; tgt.spacing=defaultSpacing; }

  void setAutoblinker(bool a, int i, int v) { autoblinker=a; blinkInterval=i; blinkIntervalVariation=v; }
  void setAutoblinker(bool a)               { autoblinker=a; }
  void setIdleMode(bool a, int i, int v)    { idle=a; idleInterval=i; idleIntervalVariation=v; }
  void setIdleMode(bool a)                  { idle=a; }
  void setCuriosity(bool v)  { curious_mode=v; }
  void setCyclops(bool)      { /* not implemented in v6 */ }
  void setSweat(bool v)      { sweat=v; }
  void setHFlicker(bool v, byte a) { hFlicker=v; hFlickerAmp=a; }
  void setHFlicker(bool v)         { hFlicker=v; }
  void setVFlicker(bool v, byte a) { vFlicker=v; vFlickerAmp=a; }
  void setVFlicker(bool v)         { vFlicker=v; }

  // ── setPosition() ─────────────────────────────────────────────────────────
  void setPosition(unsigned char pos) {
    float maxX = screenWidth  - defaultWidth*2 - tgt.spacing;
    float maxY = screenHeight - defaultHeight;
    float cx   = maxX/2.0f, cy = maxY/2.0f;
    switch(pos){
      case POS_N:  eyeLxTarget=cx;   eyeLyTarget=0;   break;
      case POS_NE: eyeLxTarget=maxX; eyeLyTarget=0;   break;
      case POS_E:  eyeLxTarget=maxX; eyeLyTarget=cy;  break;
      case POS_SE: eyeLxTarget=maxX; eyeLyTarget=maxY;break;
      case POS_S:  eyeLxTarget=cx;   eyeLyTarget=maxY;break;
      case POS_SW: eyeLxTarget=0;    eyeLyTarget=maxY;break;
      case POS_W:  eyeLxTarget=0;    eyeLyTarget=cy;  break;
      case POS_NW: eyeLxTarget=0;    eyeLyTarget=0;   break;
      default: eyeLxTarget=cx;   eyeLyTarget=cy;  break;
    }
  }

  // ── Blink / open / close ──────────────────────────────────────────────────
  void close()            { _startBlink(0); eyeL_open=false; eyeR_open=false; }
  void open()             { eyeL_open=true; eyeR_open=true; blinkTTarget=0.0f; }
  void blink()            { _startBlink(0); eyeL_open=true; eyeR_open=true; }
  void blink(bool l, bool r) { if(l||r) _startBlink(0); }
  void close(bool l, bool r) { close(); }
  void open(bool l, bool r)  { open(); }

  // ── One-shot animations ───────────────────────────────────────────────────
  void anim_confused() { confused=true; }
  void anim_laugh()    { laugh=true; }

  void anim_shake() {
    if(shakePhase!=0) return;
    shakePhase=1; shakeTimer=millis();
    orbitStep=0; orbitCount=0; dazedBlinks=0;
    setHFlicker(true,18);
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  drawEyes() — main render function
  // ══════════════════════════════════════════════════════════════════════════
  void drawEyes() {
    unsigned long now = millis();

    // ── 1. BREATHING ─────────────────────────────────────────────────────────
    breathPhase += breathSpeed;
    if(breathPhase > TWO_PI) breathPhase -= TWO_PI;
    float breathW = sinf(breathPhase) * breathAmp;

    // ── 2. BOUNCE (happy/excited) ─────────────────────────────────────────────
    if(bounceAmp > 0.01f) {
      bouncePhase += bounceSpeed;
      if(bouncePhase > TWO_PI) bouncePhase -= TWO_PI;
    }
    float bounceY = -fabsf(sinf(bouncePhase)) * bounceAmp;

    // ── 3. SAD TREMBLE ────────────────────────────────────────────────────────
    if(sadTrembleAmp > 0.01f) {
      sadTremblePhase += 0.04f;
    }
    float sadTremble = sinf(sadTremblePhase * 0.3f) * sadTrembleAmp;

    // ── 4. SURPRISED OVERSHOOT DECAY ─────────────────────────────────────────
    surprisedOvershoot *= 0.82f;

    // ── 5. MICRO-EXPRESSION ENGINE ────────────────────────────────────────────
    if(now > nextMicro) {
      _fireMicro(now);
      nextMicro = now + random(5000, 10000);
    }
    // Decay micro overrides
    if(now > microEndTime) {
      microWidthBoost   = zlerpf(microWidthBoost,   0, 0.1f);
      microHeightBoost  = zlerpf(microHeightBoost,  0, 0.1f);
      microTiltBoost    = zlerpf(microTiltBoost,     0, 0.1f);
      microSpacingBoost = zlerpf(microSpacingBoost,  0, 0.1f);
    }

    // ── 6. AUTOBLINKER ────────────────────────────────────────────────────────
    if(autoblinker && !blinkClosing && !blinkOpening && now >= nextBlink) {
      // Choose blink variant based on emotion
      int variant = 0;
      if(currentEmotion==EMOTION_SLEEPY)                 variant=3;
      else if(currentEmotion==EMOTION_SAD)               variant=1;
      else if(random(8)==0)                               variant=2; // occasional double
      _startBlink(variant);
    }

    // ── 7. BLINK UPDATE ───────────────────────────────────────────────────────
    _updateBlink(now);

    // ── 8. IDLE GAZE WANDER ───────────────────────────────────────────────────
    if(idle && now >= idleTimer) {
      float maxX = screenWidth  - defaultWidth*2 - tgt.spacing;
      float maxY = screenHeight - defaultHeight;
      eyeLxTarget = random((int)maxX);
      eyeLyTarget = random((int)maxY);
      idleTimer = now + (unsigned long)(idleInterval*1000) + (unsigned long)(random(idleIntervalVariation)*1000);
    }

    // Curiosity mode: outer eye gets taller when looking far left/right
    float curiosBoostL=0, curiosBoostR=0;
    if(curious_mode) {
      if(eyeLx <= 12)                                  curiosBoostL = 8;
      if(eyeRx >= screenWidth - defaultWidth - 12)    curiosBoostR = 8;
    }

    // ── 9. INTERPOLATE POSITION ───────────────────────────────────────────────
    eyeLx = zlerpf(eyeLx, eyeLxTarget, lerpPos);
    eyeLy = zlerpf(eyeLy, eyeLyTarget, lerpPos);
    eyeRx = eyeLx + cur.eyeWidth + cur.spacing;
    eyeRy = eyeLy;

    // ── 10. INTERPOLATE FaceParameters ───────────────────────────────────────
    cur.eyeWidth       = zlerpf(cur.eyeWidth,       tgt.eyeWidth,       lerpShape);
    cur.eyeHeight      = zlerpf(cur.eyeHeight,      tgt.eyeHeight,      lerpShape);
    cur.cornerRadius   = zlerpf(cur.cornerRadius,   tgt.cornerRadius,   lerpShape);
    cur.spacing        = zlerpf(cur.spacing,        tgt.spacing,        lerpPos);
    cur.lidTopL        = zlerpf(cur.lidTopL,        tgt.lidTopL,        lerpLid);
    cur.lidTopR        = zlerpf(cur.lidTopR,        tgt.lidTopR,        lerpLid);
    cur.lidBottomL     = zlerpf(cur.lidBottomL,     tgt.lidBottomL,     lerpLid);
    cur.lidBottomR     = zlerpf(cur.lidBottomR,     tgt.lidBottomR,     lerpLid);
    cur.tiltL          = zlerpf(cur.tiltL,          tgt.tiltL,          lerpTilt);
    cur.tiltR          = zlerpf(cur.tiltR,          tgt.tiltR,          lerpTilt);
    cur.browHeightL    = zlerpf(cur.browHeightL,    tgt.browHeightL,    lerpBrow);
    cur.browHeightR    = zlerpf(cur.browHeightR,    tgt.browHeightR,    lerpBrow);
    cur.browAngleL     = zlerpf(cur.browAngleL,     tgt.browAngleL,     lerpBrow);
    cur.browAngleR     = zlerpf(cur.browAngleR,     tgt.browAngleR,     lerpBrow);
    cur.yOffset        = zlerpf(cur.yOffset,        tgt.yOffset,        lerpPos);
    cur.eyeHeightR_scale = zlerpf(cur.eyeHeightR_scale, tgt.eyeHeightR_scale, lerpAsym);
    cur.eyeWidthR_scale  = zlerpf(cur.eyeWidthR_scale,  tgt.eyeWidthR_scale,  lerpAsym);

    // ── 11. MACRO ANIMATIONS (FluxGarage compat) ──────────────────────────────
    if(confused){
      if(confusedToggle){ setHFlicker(true,20); confusedTimer=now; confusedToggle=false; }
      else if(now-confusedTimer>=500){ setHFlicker(false,0); confusedToggle=true; confused=false; }
    }
    if(laugh){
      if(laughToggle){ setVFlicker(true,5); laughTimer=now; laughToggle=false; }
      else if(now-laughTimer>=500){ setVFlicker(false,0); laughToggle=true; laugh=false; }
    }

    // Shake state machine
    _updateShake(now);

    // H/V flicker
    if(hFlicker){ int o=hFlickerAlt?hFlickerAmp:-hFlickerAmp; eyeLx+=o; eyeRx+=o; hFlickerAlt=!hFlickerAlt; }
    if(vFlicker){ int o=vFlickerAlt?vFlickerAmp:-vFlickerAmp; eyeLy+=o; eyeRy+=o; vFlickerAlt=!vFlickerAlt; }

    // ── 12. COMPUTE FINAL DRAW VALUES ─────────────────────────────────────────
    // Apply breathing, bounce, tremble, overshoot, micro overrides on top of cur
    float W  = cur.eyeWidth  + breathW + microWidthBoost;
    float HL = cur.eyeHeight + surprisedOvershoot + bounceY + microHeightBoost + curiosBoostL;
    float HR = (cur.eyeHeight + surprisedOvershoot + bounceY + microHeightBoost + curiosBoostR) * cur.eyeHeightR_scale;
    float WR = W * cur.eyeWidthR_scale;
    float R  = constrain(cur.cornerRadius, 2.0f, W/2.0f);
    float sp = cur.spacing + microSpacingBoost;

    // Y position with sad drift and bounce
    float ly = eyeLy + cur.yOffset + sadTremble + bounceY;
    float ry = eyeRy + cur.yOffset + sadTremble + bounceY;

    // Blink overrides final lid values
    float blinkAdd = blinkT;  // 0=open, 1=closed
    float topLL = constrain(cur.lidTopL  + blinkAdd, 0, 1);
    float topRL = constrain(cur.lidTopR  + blinkAdd, 0, 1);
    float botLL = constrain(cur.lidBottomL, 0, 1);
    float botRL = constrain(cur.lidBottomR, 0, 1);

    // Eyelid pixel heights
    int lidTL = (int)(topLL * HL);
    int lidTR = (int)(topRL * HR);
    int lidBL = (int)(botLL * HL);
    int lidBR = (int)(botRL * HR);

    // Tilt amounts (pixels of asymmetric corner clip)
    // tilt > 0 → inner top corner droops (angry/sad)
    // tilt < 0 → outer top corner droops (tired)
    float tiltScale = HL * 0.45f;
    float tL = cur.tiltL + microTiltBoost;
    float tR = cur.tiltR - microTiltBoost;  // mirror for right eye

    int tiltTL_inner = (tL > 0) ? (int)(tL  * tiltScale) : 0;
    int tiltTL_outer = (tL < 0) ? (int)(-tL * tiltScale) : 0;
    int tiltTR_inner = (tR > 0) ? (int)(tR  * tiltScale) : 0;
    int tiltTR_outer = (tR < 0) ? (int)(-tR * tiltScale) : 0;

    // Integer coords
    int lx=(int)eyeLx, ly_i=(int)ly;
    int rx=(int)(eyeLx+W+sp), ry_i=(int)ry;
    int lw=(int)W, lh=(int)HL;
    int rw=(int)WR, rh=(int)HR;
    int cr=(int)R;

    // ── 13. DRAW ──────────────────────────────────────────────────────────────
    display->clearDisplay();

    // Eye bodies
    display->fillRoundRect(lx, ly_i, lw, lh, cr, MAINCOLOR);
    display->fillRoundRect(rx, ry_i, rw, rh, cr, MAINCOLOR);

    // Eyelid clips — all drawn in BGCOLOR over the white eye

    // Top lids (sleepy / angry / mood)
    if(lidTL > 0)
      display->fillRect(lx-1, ly_i-1, lw+2, lidTL+1, BGCOLOR);
    if(lidTR > 0)
      display->fillRect(rx-1, ry_i-1, rw+2, lidTR+1, BGCOLOR);

    // Bottom lids (happy smile clip — FluxGarage style)
    if(lidBL > 0)
      display->fillRoundRect(lx-1, ly_i+lh-lidBL, lw+2, (int)defaultHeight, cr, BGCOLOR);
    if(lidBR > 0)
      display->fillRoundRect(rx-1, ry_i+rh-lidBR, rw+2, (int)defaultHeight, cr, BGCOLOR);

    // Tilt clips — triangles on top corners
    // LEFT EYE tilt
    if(tiltTL_inner > 0)  // inner = right side of left eye
      display->fillTriangle(lx, ly_i-1, lx+lw, ly_i-1, lx+lw, ly_i+tiltTL_inner-1, BGCOLOR);
    if(tiltTL_outer > 0)  // outer = left side of left eye
      display->fillTriangle(lx, ly_i-1, lx+lw, ly_i-1, lx, ly_i+tiltTL_outer-1, BGCOLOR);

    // RIGHT EYE tilt (mirrored)
    if(tiltTR_inner > 0)  // inner = left side of right eye
      display->fillTriangle(rx, ry_i-1, rx+rw, ry_i-1, rx, ry_i+tiltTR_inner-1, BGCOLOR);
    if(tiltTR_outer > 0)  // outer = right side of right eye
      display->fillTriangle(rx, ry_i-1, rx+rw, ry_i-1, rx+rw, ry_i+tiltTR_outer-1, BGCOLOR);

    // SAD: additional bottom-outer corner clips for the "melting" look
    if(currentEmotion==EMOTION_SAD && cur.lidTopL > 0.05f) {
      int sh = (int)(cur.lidTopL * HL * 0.6f);
      if(sh>0){
        display->fillTriangle(lx, ly_i+lh, lx+sh, ly_i+lh, lx, ly_i+lh-sh, BGCOLOR);
        display->fillTriangle(rx+rw, ry_i+rh, rx+rw-sh, ry_i+rh, rx+rw, ry_i+rh-sh, BGCOLOR);
      }
    }

    // ── 14. EYEBROWS ─────────────────────────────────────────────────────────
    _drawEyebrow(lx, ly_i, lw, cur.browHeightL, cur.browAngleL, false);
    _drawEyebrow(rx, ry_i, rw, cur.browHeightR, cur.browAngleR, true);

    // ── 15. SWEAT DROPS ───────────────────────────────────────────────────────
    if(sweat) _drawSweat();

    display->display();
  }

private:

  // ══════════════════════════════════════════════════════════════════════════
  //  _neutralParams — fill a FaceParameters with calm default values
  // ══════════════════════════════════════════════════════════════════════════
  void _neutralParams(FaceParameters &p) {
    p.eyeWidth        = defaultWidth;
    p.eyeHeight       = defaultHeight;
    p.cornerRadius    = defaultRadius;
    p.spacing         = defaultSpacing;
    p.lidTopL         = 0.0f;
    p.lidTopR         = 0.0f;
    p.lidBottomL      = 0.0f;
    p.lidBottomR      = 0.0f;
    p.tiltL           = 0.0f;
    p.tiltR           = 0.0f;
    p.browHeightL     = 6.0f;
    p.browHeightR     = 6.0f;
    p.browAngleL      = 0.0f;
    p.browAngleR      = 0.0f;
    p.yOffset         = 0.0f;
    p.eyeHeightR_scale= 1.0f;
    p.eyeWidthR_scale = 1.0f;
    p.motionEnergy    = 0.3f;
    p.intensity       = 1.0f;
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  _buildTarget — compute target FaceParameters for a given emotion
  // ══════════════════════════════════════════════════════════════════════════
  void _buildTarget(int emotion, float I) {
    // Start from neutral, then modify
    _neutralParams(tgt);
    tgt.intensity = I;

    // Reset animation drivers
    bounceAmp   = 0.0f;
    bounceSpeed = 0.0f;
    sadTrembleAmp = 0.0f;
    lovingActive  = false;

    // Adjust lerp speeds per emotion physics
    // Happy/Excited → quick. Sad/Sleepy → slow. Angry → snappy.
    switch(emotion) {
      case EMOTION_HAPPY:
      case EMOTION_EXCITED:
        lerpShape=0.25f; lerpLid=0.28f; lerpTilt=0.25f; lerpBrow=0.22f;
        break;
      case EMOTION_ANGRY:
        lerpShape=0.35f; lerpLid=0.38f; lerpTilt=0.35f; lerpBrow=0.35f;
        break;
      case EMOTION_SAD:
      case EMOTION_SLEEPY:
        lerpShape=0.10f; lerpLid=0.10f; lerpTilt=0.10f; lerpBrow=0.09f;
        break;
      case EMOTION_CURIOUS:
      case EMOTION_THINKING:
        lerpShape=0.14f; lerpLid=0.15f; lerpTilt=0.14f; lerpBrow=0.13f;
        break;
      default:
        lerpShape=0.18f; lerpLid=0.20f; lerpTilt=0.16f; lerpBrow=0.14f;
        break;
    }

    switch(emotion) {

      // ── NEUTRAL ─────────────────────────────────────────────────────────
      case EMOTION_NEUTRAL:
        // Everything already at neutral defaults
        breathSpeed = 0.018f;
        breathAmp   = 0.8f;
        break;

      // ── HAPPY ───────────────────────────────────────────────────────────
      case EMOTION_HAPPY:
        tgt.eyeWidth     = defaultWidth  + 4*I;
        tgt.eyeHeight    = defaultHeight - 2*I;   // slight vertical squash = smile
        tgt.cornerRadius = defaultRadius + 2*I;
        tgt.lidBottomL   = 0.32f * I;             // FluxGarage smile clip
        tgt.lidBottomR   = 0.32f * I;
        tgt.browHeightL  = 8.0f;
        tgt.browHeightR  = 8.0f;
        tgt.browAngleL   =  5.0f * I;             // outer ends up = cheerful
        tgt.browAngleR   = -5.0f * I;
        bounceAmp        = 2.0f * I;
        bounceSpeed      = 0.12f;
        breathAmp        = 1.2f;
        break;

      // ── EXCITED ─────────────────────────────────────────────────────────
      case EMOTION_EXCITED:
        tgt.eyeWidth     = defaultWidth  + 7*I;
        tgt.eyeHeight    = defaultHeight + 6*I;
        tgt.cornerRadius = defaultRadius + 3*I;
        tgt.spacing      = defaultSpacing + 3*I;
        tgt.browHeightL  = 10.0f;
        tgt.browHeightR  = 10.0f;
        tgt.browAngleL   =  8.0f * I;
        tgt.browAngleR   = -8.0f * I;
        bounceAmp        = 4.0f * I;
        bounceSpeed      = 0.20f;
        breathAmp        = 1.5f;
        // Spring overshoot on entry
        if(!surprisedSpringFired){ surprisedOvershoot=10.0f*I; surprisedSpringFired=true; }
        break;

      // ── CURIOUS ─────────────────────────────────────────────────────────
      case EMOTION_CURIOUS:
        tgt.eyeWidth     = defaultWidth  + 2*I;
        tgt.eyeHeight    = defaultHeight + 3*I;
        tgt.spacing      = defaultSpacing - 3*I;
        tgt.tiltL        = -0.25f * I;            // left eye tilts outward/up
        tgt.tiltR        =  0.0f;
        tgt.browHeightL  = 9.0f;
        tgt.browHeightR  = 6.0f;
        tgt.browAngleL   = -8.0f * I;             // left brow raises outward
        tgt.browAngleR   =  2.0f * I;
        breathAmp        = 0.6f;
        break;

      // ── THINKING ────────────────────────────────────────────────────────
      case EMOTION_THINKING:
        tgt.eyeWidth     = defaultWidth  - 2*I;
        tgt.eyeHeight    = defaultHeight - 4*I;
        tgt.spacing      = defaultSpacing - 2*I;
        tgt.tiltL        =  0.15f * I;            // slight inward lean
        tgt.tiltR        =  0.0f;
        tgt.lidTopL      =  0.10f * I;
        tgt.browHeightL  = 7.0f;
        tgt.browHeightR  = 7.0f;
        tgt.browAngleL   =  4.0f * I;
        tgt.browAngleR   = -1.0f * I;
        breathAmp        = 0.4f;
        break;

      // ── SAD ─────────────────────────────────────────────────────────────
      case EMOTION_SAD:
        tgt.eyeWidth     = defaultWidth  - 2*I;
        tgt.eyeHeight    = defaultHeight - 5*I;
        tgt.spacing      = defaultSpacing - 3*I;
        tgt.tiltL        =  0.4f * I;             // inner corners droop
        tgt.tiltR        =  0.4f * I;
        tgt.lidTopL      =  0.20f * I;
        tgt.lidTopR      =  0.20f * I;
        tgt.yOffset      =  5.0f * I;             // whole face drifts down
        tgt.browHeightL  = 7.0f;
        tgt.browHeightR  = 7.0f;
        tgt.browAngleL   =  12.0f * I;            // inner ends droop sharply
        tgt.browAngleR   = -12.0f * I;
        sadTrembleAmp    = 0.6f * I;
        breathAmp        = 0.5f;
        breathSpeed      = 0.010f;                // slower breathing
        break;

      // ── SLEEPY ──────────────────────────────────────────────────────────
      case EMOTION_SLEEPY:
        tgt.eyeWidth     = defaultWidth;
        tgt.eyeHeight    = defaultHeight;
        tgt.lidTopL      = 0.55f * I;             // heavy drooping lids
        tgt.lidTopR      = 0.55f * I;
        tgt.tiltL        = -0.15f * I;            // outer corners droop
        tgt.tiltR        = -0.15f * I;
        tgt.browHeightL  = 5.0f;
        tgt.browHeightR  = 5.0f;
        tgt.browAngleL   = -4.0f * I;
        tgt.browAngleR   =  4.0f * I;
        breathSpeed      = 0.010f;
        breathAmp        = 1.0f;
        break;

      // ── ANGRY ───────────────────────────────────────────────────────────
      case EMOTION_ANGRY:
        tgt.eyeWidth     = defaultWidth  + 2*I;
        tgt.eyeHeight    = defaultHeight - 2*I;
        tgt.spacing      = defaultSpacing - 2*I;
        tgt.tiltL        =  0.7f * I;             // sharp inner droop
        tgt.tiltR        =  0.7f * I;
        tgt.lidTopL      =  0.35f * I;
        tgt.lidTopR      =  0.35f * I;
        tgt.browHeightL  = 8.0f;
        tgt.browHeightR  = 8.0f;
        tgt.browAngleL   =  20.0f * I;            // severe inward V
        tgt.browAngleR   = -20.0f * I;
        breathAmp        = 0.3f;
        break;

      // ── SURPRISED ───────────────────────────────────────────────────────
      case EMOTION_SURPRISED:
        tgt.eyeWidth     = defaultWidth  + 8*I;
        tgt.eyeHeight    = defaultHeight + 10*I;
        tgt.cornerRadius = (byte)(defaultRadius + 4*I);
        tgt.spacing      = defaultSpacing + 4*I;
        tgt.browHeightL  = 12.0f;
        tgt.browHeightR  = 12.0f;
        tgt.browAngleL   =  2.0f * I;
        tgt.browAngleR   = -2.0f * I;
        // Spring overshoot on first entry
        if(!surprisedSpringFired){ surprisedOvershoot=14.0f*I; surprisedSpringFired=true; }
        breathAmp        = 0.5f;
        break;

      // ── SUSPICIOUS ──────────────────────────────────────────────────────
      case EMOTION_SUSPICIOUS:
        tgt.eyeWidth       = defaultWidth;
        tgt.eyeHeight      = defaultHeight - 2*I;
        tgt.spacing        = defaultSpacing;
        tgt.tiltL          =  0.35f * I;          // left eye inner droop
        tgt.tiltR          =  0.0f;
        tgt.lidTopL        =  0.28f * I;          // left eye more lidded
        tgt.lidTopR        =  0.08f * I;
        tgt.eyeHeightR_scale = 1.0f + 0.08f*I;   // right eye slightly taller
        tgt.browHeightL    = 7.0f;
        tgt.browHeightR    = 7.0f;
        tgt.browAngleL     =  14.0f * I;
        tgt.browAngleR     = -4.0f * I;
        breathAmp          = 0.4f;
        break;
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  _startBlink — initiate a blink of given variant
  //  0=normal, 1=slow emotional, 2=double, 3=sleepy long
  // ══════════════════════════════════════════════════════════════════════════
  void _startBlink(int variant) {
    blinkVariant   = variant;
    blinkClosing   = true;
    blinkOpening   = false;
    blinkTTarget   = 1.0f;
    doubleBlink2nd = false;

    switch(variant){
      case 0:  blinkCloseSpeed=0.45f; blinkOpenSpeed=0.35f; break;
      case 1:  blinkCloseSpeed=0.22f; blinkOpenSpeed=0.18f; break; // slow emotional
      case 2:  blinkCloseSpeed=0.50f; blinkOpenSpeed=0.45f; break; // double
      case 3:  blinkCloseSpeed=0.18f; blinkOpenSpeed=0.12f; break; // sleepy
    }

    nextBlink = millis()
              + (unsigned long)(blinkInterval*1000)
              + (unsigned long)(random(blinkIntervalVariation)*1000);
  }

  // ── Advance blink state each frame ────────────────────────────────────────
  void _updateBlink(unsigned long now) {
    if(blinkClosing) {
      blinkT = zlerpf(blinkT, 1.0f, blinkCloseSpeed);
      if(blinkT > 0.92f) {
        blinkT       = 1.0f;
        blinkClosing = false;
        blinkOpening = true;
        blinkTTarget = 0.0f;
        if(blinkVariant==3) delay(0); // sleepy: hold closed a tiny moment
        if(blinkVariant==2 && !doubleBlink2nd) {
          doubleBlink2ndAt = now + 120;
        }
      }
    }
    if(blinkOpening) {
      blinkT = zlerpf(blinkT, 0.0f, blinkOpenSpeed);
      if(blinkT < 0.05f) {
        blinkT       = 0.0f;
        blinkOpening = false;
        if(blinkVariant==2 && !doubleBlink2nd && now >= doubleBlink2ndAt) {
          doubleBlink2nd = true;
          blinkClosing   = true;
          blinkTTarget   = 1.0f;
        }
      }
    }
    // Keep open when eyeL/R_open flags set
    if(eyeL_open && eyeR_open && !blinkClosing && !blinkOpening) {
      blinkT = zlerpf(blinkT, 0.0f, 0.4f);
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  _fireMicro — random subtle life behavior
  // ══════════════════════════════════════════════════════════════════════════
  void _fireMicro(unsigned long now) {
    // Pick an action, avoid repeating last one
    int action;
    do { action = random(6); } while(action == lastMicro);
    lastMicro = action;

    microEndTime = now + random(300, 800);

    switch(action) {
      case 0: microWidthBoost   =  random(2,4);  break;  // slight eye widen
      case 1: microHeightBoost  =  random(2,5);  break;  // slight eye tall
      case 2: microHeightBoost  = -random(2,4);  break;  // slight eye squish
      case 3: microTiltBoost    =  (random(2) ? 0.12f : -0.12f); break; // micro tilt
      case 4: microSpacingBoost =  random(2,4);  break;  // eyes drift apart
      case 5: _startBlink(random(2));             break;  // surprise blink variant
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  _drawEyebrow — thin minimal horizontal bar with angle
  //
  //  Drawn as a 3px thick filled rect, then corner triangles clip it to angle.
  //  browAngle > 0 → outer end rises (happy), < 0 → outer end falls (sad/angry)
  //  isRight: true = right eye brow (angle mirrors)
  // ══════════════════════════════════════════════════════════════════════════
  void _drawEyebrow(int ex, int ey, int ew, float browH, float browAngle, bool isRight) {
    if(browH < 1.0f) return;

    int by    = ey - (int)browH;          // top of eyebrow bar
    int bh    = 3;                         // bar thickness (pixels)
    int rise  = (int)(tanf(browAngle * PI / 180.0f) * (ew / 2.0f));
    if(isRight) rise = -rise;             // mirror for right eye

    // Draw the bar as a filled rect
    display->fillRect(ex, by, ew, bh, MAINCOLOR);

    // Now clip corners to simulate the angle using BGCOLOR triangles
    if(rise > 0) {
      // Outer end rises: clip inner-bottom corner
      display->fillTriangle(ex,      by+bh, ex+rise,    by+bh, ex,      by+bh-rise,  BGCOLOR);
      display->fillTriangle(ex+ew,   by,    ex+ew-rise, by,    ex+ew,   by+rise,     BGCOLOR);
    } else if(rise < 0) {
      int r = -rise;
      // Outer end falls: clip outer-bottom corner
      display->fillTriangle(ex+ew,   by+bh, ex+ew-r, by+bh, ex+ew,   by+bh-r,   BGCOLOR);
      display->fillTriangle(ex,      by,    ex+r,     by,    ex,      by+r,       BGCOLOR);
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  _updateShake — anim_shake state machine
  // ══════════════════════════════════════════════════════════════════════════
  void _updateShake(unsigned long now) {
    if(shakePhase==0) return;
    if(shakePhase==1){
      if(now-shakeTimer>=600){
        setHFlicker(false,0);
        shakePhase=2; orbitStep=0; orbitCount=0; orbitTimer=now;
        setPosition(ORBIT[0]);
      }
    } else if(shakePhase==2){
      if(now-orbitTimer>=120){
        orbitTimer=now; orbitStep++;
        if(orbitStep>=8){ orbitStep=0; orbitCount++; }
        if(orbitCount>=2){ setPosition(DEFAULT); shakePhase=3; dazedBlinks=0; dazedTimer=now; }
        else setPosition(ORBIT[orbitStep]);
      }
    } else if(shakePhase==3){
      if(now-dazedTimer>=300){ dazedTimer=now; dazedBlinks++;
        if(dazedBlinks<=3) blink(); else { shakePhase=0; setPosition(DEFAULT); }
      }
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  //  _drawSweat — FluxGarage-identical sweat drop animation
  // ══════════════════════════════════════════════════════════════════════════
  void _drawSweat(){
    auto _drop = [&](int &xi, int &xp, float &yp, int &ym, float &sw, float &sh){
      if(yp<=ym){yp+=0.5f;}
      else{xi=random(screenWidth);yp=2;ym=random(10)+10;sw=1;sh=2;}
      if(yp<=ym/2){sw+=0.5f;sh+=0.5f;}else{sw-=0.1f;sh-=0.5f;}
      xp=xi-(int)(sw/2);
      display->fillRoundRect(xp,(int)yp,(int)sw,(int)sh,sweatRadius,MAINCOLOR);
    };
    _drop(s1xi,s1x,s1y,s1ymax,s1w,s1h);
    _drop(s2xi,s2x,s2y,s2ymax,s2w,s2h);
    _drop(s3xi,s3x,s3y,s3ymax,s3w,s3h);
  }

}; // end class CustomEyes

#endif // CUSTOM_EYE_ENGINE_H
