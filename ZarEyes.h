// ============================================================================
// ZarEyes.h — Noura Robotics
// Expression engine for Zar — SSD1306 128×64 OLED
// Ported from ZarEyes_Noura.html
// Usage:
//   ZarEyes eyes(display);
//   eyes.begin();
//   eyes.setExpression("HAPPY");
//   eyes.update();   // call every loop()
// ============================================================================

#pragma once
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include <string.h>

// ── Canvas dimensions (matches OLED) ────────────────────────────────────────
#define ZE_W 128
#define ZE_H 64

class ZarEyes {
public:

  // ── Expression IDs ─────────────────────────────────────────────────────────
  enum Expr {
    EXPR_DEFAULT = 0,
    EXPR_HAPPY,
    EXPR_SAD,
    EXPR_ANGRY,
    EXPR_CURIOUS,
    EXPR_TIRED,
    EXPR_SURPRISED,
    EXPR_ANXIOUS,
    EXPR_LOVE,
    EXPR_CONFUSED,
    EXPR_EXCITED,
    EXPR_BORED,
    EXPR_SHY,
    EXPR_SLEEP,
    EXPR_DIZZY,
    EXPR_COUNT
  };

  // ── Mood parameters (mirrors JS MOODS table) ──────────────────────────────
  struct MoodDef {
    float eyeW, eyeH, r, space;
    bool  tired, angry, happy, sleep;
    bool  sad, surprised, anxious, love;
    bool  confused, excited, sarcastic, shy, dizzy;
  };

  static const MoodDef MOODS[EXPR_COUNT];

  // ── Constructor ────────────────────────────────────────────────────────────
  ZarEyes(Adafruit_SSD1306& disp) : _disp(disp) {}

  // ── begin() — call once in setup() ────────────────────────────────────────
  void begin() {
    _expr       = EXPR_DEFAULT;
    _eyeW       = 46; _eyeH = 46; _eyeR = 10; _space = 18;
    _tiredH     = 0;  _angryH = 0; _happyOff = 0; _sleepOff = 0; _sadH = 0;
    _moodProg   = 1.0f;
    _blinkOn    = true;
    _blinkTimer = 2.5f;
    _blinkH     = 0;  _blinkDir = 0;
    _idleOn     = true;
    _idleTimer  = 2.0f;
    _LX = 0; _LY = 0; _LXNext = 0; _LYNext = 0;
    _dizzyAngle = 0;
    _sleepBreath = 0;
    _surprisedScale = 0; _surprisedSparkle = 0; _surprisedBounce = 0;
    _anxiousDartX = 0; _anxiousDartTarget = 0; _anxiousDartTimer = 0;
    _anxiousBrowShake = 0;
    _excitedRoamLX = 0; _excitedRoamLY = 0;
    _excitedRoamRX = 0; _excitedRoamRY = 0;
    _excitedRoamTimer = 0;
    _excitedLeftLeads = true;
    _tiredPulse = 0; _tiredPulseDir = 0;
    _lastMs = millis();
  }

  // ── setExpression() ────────────────────────────────────────────────────────
  void setExpression(const char* name) {
    Expr e = nameToExpr(name);
    if (e != _expr) {
      _expr     = e;
      _moodProg = 0;
      if (_expr == EXPR_ANGRY) _angryEntry = 0.001f;
      else _angryEntry = 0;
      if (_expr == EXPR_SURPRISED) { _surprisedScale = 0.05f; _surprisedBounce = 0; _surprisedSparkle = 0; }
    }
  }

  void setExpression(Expr e) {
    if (e != _expr) {
      _expr     = e;
      _moodProg = 0;
      if (_expr == EXPR_ANGRY) _angryEntry = 0.001f;
      else _angryEntry = 0;
      if (_expr == EXPR_SURPRISED) { _surprisedScale = 0.05f; _surprisedBounce = 0; _surprisedSparkle = 0; }
    }
  }

  // ── Trigger one-shot animations ───────────────────────────────────────────
  void animLaugh()   { _laughActive = true;  _laughTimer = 0; }
  void animWink()    { _blinkDir = 1; }
  void animGlitch()  { _glitchActive = true; _glitchTimer = 0; }
  void animDizzyShake() { _dizzyShakeActive = true; _dizzyShakeTimer = 0; _dizzyShakeAngle = 0; }

  // ── Feature toggles ───────────────────────────────────────────────────────
  void setBlinkEnabled(bool on) { _blinkOn = on; }
  void setIdleEnabled(bool on)  { _idleOn  = on; if (!on) { _LXNext = 0; _LYNext = 0; } }

  // ── update() — call every loop() ──────────────────────────────────────────
  void update() {
    unsigned long now = millis();
    float dt = (now - _lastMs) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    _lastMs = now;

    const MoodDef& m = MOODS[_expr];

    // ── Smooth lerp eye shape ──────────────────────────────────────────────
    float lf = 1.0f - powf(0.15f, dt);
    _eyeW  += (m.eyeW  - _eyeW)  * lf;
    _eyeH  += (m.eyeH  - _eyeH)  * lf;
    _eyeR  += (m.r     - _eyeR)  * lf;
    _space += (m.space - _space) * lf;

    // ── Tween eyelid overlays ──────────────────────────────────────────────
    _tiredH   = _tween(_tiredH,   m.tired   ? _eyeH/2 : 0);
    _sadH     = _tween(_sadH,     m.sad     ? _eyeH/2 : 0);
    _angryH   = _tween(_angryH,   m.angry   ? _eyeH/2 : 0);
    _happyOff = _tween(_happyOff, m.happy   ? _eyeH/2 + 3 : 0);
    _sleepOff = _tween(_sleepOff, m.sleep   ? _eyeH - 4 : 0);

    // ── Mood progress ──────────────────────────────────────────────────────
    _moodProg = fminf(1.0f, _moodProg + dt * 1.8f);

    // ── Idle wander ────────────────────────────────────────────────────────
    if (_idleOn && !_dizzyShakeActive && !m.sleep && !m.curious && !m.anxious) {
      _idleTimer -= dt;
      if (_idleTimer <= 0) {
        float maxX = (ZE_W - (_eyeW * 2 + _space)) * 0.18f;
        float maxY = (ZE_H - _eyeH) * 0.22f;
        _LXNext = ((float)random(-100, 100) / 100.0f) * maxX;
        _LYNext = ((float)random(-100, 100) / 100.0f) * maxY;
        _idleTimer = m.tired ? 4.0f + (float)random(0, 300)/100.0f
                             : 1.5f + (float)random(0, 200)/100.0f;
      }
    }

    // ── Position tween ────────────────────────────────────────────────────
    if (_dizzyShakeActive) {
      _LX = _LXNext; _LY = _LYNext;
    } else if (m.curious) {
      _LX += (_LXNext - _LX) * 0.35f;
      _LY += (_LYNext - _LY) * 0.35f;
    } else if (m.tired) {
      _LX += (_LXNext - _LX) * 0.01f;
      _LY += (_LYNext - _LY) * 0.01f;
    } else {
      _LX = _tween(_LX, _LXNext);
      _LY = _tween(_LY, _LYNext);
    }

    // ── Blink ─────────────────────────────────────────────────────────────
    float blinkSpeed = m.curious ? 18 : 12;
    float blinkClose = m.curious ? 14 : 8;
    if (_blinkOn && !m.sleep && !m.tired) {
      _blinkTimer -= dt;
      if (_blinkTimer <= 0 && _blinkDir == 0) {
        _blinkDir   = 1;
        _blinkTimer = m.curious ? 0.8f + (float)random(0,100)/100.0f
                                : 2.0f + (float)random(0,250)/100.0f;
      }
    }
    if (!m.sleep) {
      if (_blinkDir == 1) {
        _blinkH = fminf(1.0f, _blinkH + dt * blinkSpeed);
        if (_blinkH >= 1.0f) _blinkDir = -1;
      } else if (_blinkDir == -1) {
        _blinkH = fmaxf(0.0f, _blinkH - dt * blinkClose);
        if (_blinkH <= 0.0f) { _blinkH = 0; _blinkDir = 0; }
      }
    } else {
      _blinkH = 0; _blinkDir = 0;
    }

    // ── Laugh anim ────────────────────────────────────────────────────────
    if (_laughActive) {
      _laughTimer += dt;
      _vFlicker = sinf(_laughTimer * 40) * 5;
      if (_laughTimer > 0.7f) { _vFlicker = 0; _laughActive = false; }
    } else { _vFlicker = 0; }

    // ── Dizzy spin ────────────────────────────────────────────────────────
    if (m.dizzy) _dizzyAngle += dt * 3.5f;

    // ── Dizzy shake anim ──────────────────────────────────────────────────
    if (_dizzyShakeActive) {
      _dizzyShakeTimer += dt;
      float dur = 2.8f;
      float prog = _dizzyShakeTimer / dur;
      float amplitude = sinf(prog * 3.14159f);
      float maxX = ZE_W/2.0f - _eyeW - 8;
      float maxY = ZE_H/2.0f - _eyeH/2 - 6;
      _dizzyShakeAngle += dt * (6 + prog * 8);
      _LX = cosf(_dizzyShakeAngle) * maxX * amplitude + sinf(_dizzyShakeTimer * 14) * 5;
      _LY = sinf(_dizzyShakeAngle) * maxY * amplitude + sinf(_dizzyShakeTimer * 10) * 4;
      if (_dizzyShakeTimer >= dur) {
        _dizzyShakeActive = false;
        _LX = 0; _LY = 0; _LXNext = 0; _LYNext = 0;
      }
    }

    // ── Sleep breath ──────────────────────────────────────────────────────
    if (m.sleep) _sleepBreath += dt * 0.6f;

    // ── Surprised scale ───────────────────────────────────────────────────
    if (m.surprised) {
      _surprisedScale  += dt * 3.0f;
      if (_surprisedScale > 1.0f) _surprisedScale = 1.0f;
      _surprisedBounce += dt;
      _surprisedSparkle += dt * 4.0f;
    } else {
      _surprisedScale = 0;
    }

    // ── Anxious dart ──────────────────────────────────────────────────────
    if (m.anxious) {
      _anxiousBrowShake += dt * 8.0f;
      _anxiousDartTimer -= dt;
      if (_anxiousDartTimer <= 0) {
        _anxiousDartTarget = ((float)random(-100, 100) / 100.0f) * (ZE_W/2.0f - _eyeW - 10);
        _anxiousDartTimer  = 0.12f + (float)random(0, 180) / 1000.0f;
      }
      _anxiousDartX += (_anxiousDartTarget - _anxiousDartX) * 0.28f;
      _LXNext = _anxiousDartX;
    } else {
      _anxiousDartX = 0; _anxiousDartTarget = 0; _anxiousDartTimer = 0;
    }

    // ── Excited roam ──────────────────────────────────────────────────────
    if (m.excited) {
      _excitedRoamTimer -= dt;
      if (_excitedRoamTimer <= 0) {
        float rx2 = ZE_W/2.0f - _eyeW - 10;
        float ry2 = ZE_H/2.0f - _eyeH/2 - 6;
        _excitedLeftLeads = !_excitedLeftLeads;
        if (_excitedLeftLeads) {
          _excitedRoamTargLX = -(0.7f + (float)random(0,30)/100.0f) * rx2;
          _excitedRoamTargLY = ((random(0,2) > 0 ? 1 : -1)) * (0.6f + (float)random(0,40)/100.0f) * ry2;
          _excitedRoamTargRX =  (0.3f + (float)random(0,40)/100.0f) * rx2;
          _excitedRoamTargRY = ((random(0,2) > 0 ? 1 : -1)) * (float)random(0,100)/100.0f * ry2;
        } else {
          _excitedRoamTargRX =  (0.7f + (float)random(0,30)/100.0f) * rx2;
          _excitedRoamTargRY = ((random(0,2) > 0 ? 1 : -1)) * (0.6f + (float)random(0,40)/100.0f) * ry2;
          _excitedRoamTargLX = -(0.3f + (float)random(0,40)/100.0f) * rx2;
          _excitedRoamTargLY = ((random(0,2) > 0 ? 1 : -1)) * (float)random(0,100)/100.0f * ry2;
        }
        _excitedRoamTimer = 0.25f + (float)random(0,25)/100.0f;
      }
      _excitedRoamLX += (_excitedRoamTargLX - _excitedRoamLX) * 0.25f;
      _excitedRoamLY += (_excitedRoamTargLY - _excitedRoamLY) * 0.25f;
      _excitedRoamRX += (_excitedRoamTargRX - _excitedRoamRX) * 0.25f;
      _excitedRoamRY += (_excitedRoamTargRY - _excitedRoamRY) * 0.25f;
    }

    // ── Glitch ────────────────────────────────────────────────────────────
    if (_glitchActive) {
      _glitchTimer += dt;
      if (_glitchTimer > 0.5f) { _glitchActive = false; _glitchTimer = 0; }
    }

    // ── Tired pulse ───────────────────────────────────────────────────────
    if (m.tired) {
      _tiredPulse += dt * 0.4f;
      if (_tiredPulse >= 1.0f) {
        _tiredPulse = 0;
        if (_tiredPulseDir == 0) _tiredPulseDir = 1;
        else _tiredPulseDir = (_tiredPulseDir == 1) ? 2 : 1;
      }
    } else {
      _tiredPulse = 0; _tiredPulseDir = 0;
    }

    _draw();
  }

  // ── Static name→enum helper ───────────────────────────────────────────────
  static Expr nameToExpr(const char* name) {
    if (strcmp(name, "HAPPY")     == 0) return EXPR_HAPPY;
    if (strcmp(name, "SAD")       == 0) return EXPR_SAD;
    if (strcmp(name, "ANGRY")     == 0) return EXPR_ANGRY;
    if (strcmp(name, "CURIOUS")   == 0) return EXPR_CURIOUS;
    if (strcmp(name, "TIRED")     == 0) return EXPR_TIRED;
    if (strcmp(name, "SURPRISED") == 0) return EXPR_SURPRISED;
    if (strcmp(name, "ANXIOUS")   == 0) return EXPR_ANXIOUS;
    if (strcmp(name, "LOVE")      == 0) return EXPR_LOVE;
    if (strcmp(name, "CONFUSED")  == 0) return EXPR_CONFUSED;
    if (strcmp(name, "EXCITED")   == 0) return EXPR_EXCITED;
    if (strcmp(name, "BORED")     == 0) return EXPR_BORED;
    if (strcmp(name, "SHY")       == 0) return EXPR_SHY;
    if (strcmp(name, "SLEEP")     == 0) return EXPR_SLEEP;
    if (strcmp(name, "DIZZY")     == 0) return EXPR_DIZZY;
    return EXPR_DEFAULT;
  }

private:
  Adafruit_SSD1306& _disp;

  // ── State ──────────────────────────────────────────────────────────────────
  Expr  _expr       = EXPR_DEFAULT;
  float _eyeW       = 46, _eyeH = 46, _eyeR = 10, _space = 18;
  float _tiredH     = 0,  _sadH = 0, _angryH = 0, _happyOff = 0, _sleepOff = 0;
  float _moodProg   = 1.0f;
  float _angryEntry = 0;
  float _LX = 0, _LY = 0, _LXNext = 0, _LYNext = 0;
  float _blinkH     = 0;
  int   _blinkDir   = 0;
  float _blinkTimer = 2.5f;
  bool  _blinkOn    = true;
  float _idleTimer  = 2.0f;
  bool  _idleOn     = true;
  float _dizzyAngle = 0;
  float _sleepBreath = 0;
  float _surprisedScale = 0, _surprisedSparkle = 0, _surprisedBounce = 0;
  float _anxiousDartX = 0, _anxiousDartTarget = 0, _anxiousDartTimer = 0;
  float _anxiousBrowShake = 0;
  float _excitedRoamLX = 0, _excitedRoamLY = 0;
  float _excitedRoamRX = 0, _excitedRoamRY = 0;
  float _excitedRoamTargLX = 0, _excitedRoamTargLY = 0;
  float _excitedRoamTargRX = 0, _excitedRoamTargRY = 0;
  float _excitedRoamTimer = 0;
  bool  _excitedLeftLeads = true;
  bool  _laughActive = false;
  float _laughTimer  = 0;
  float _vFlicker    = 0;
  float _tiredPulse  = 0;
  int   _tiredPulseDir = 0;
  bool  _dizzyShakeActive = false;
  float _dizzyShakeTimer = 0, _dizzyShakeAngle = 0;
  bool  _glitchActive = false;
  float _glitchTimer  = 0;
  unsigned long _lastMs = 0;

  // ── Tween helper ──────────────────────────────────────────────────────────
  static float _tween(float cur, float next) { return (cur + next) / 2.0f; }

  // ── Integer clamp ─────────────────────────────────────────────────────────
  static int _clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

  // ── Draw rounded rect ─────────────────────────────────────────────────────
  void _fillRR(int x, int y, int w, int h, int r) {
    if (w <= 0 || h <= 0) return;
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    _disp.fillRect(x + r, y,         w - 2*r, h,     WHITE);
    _disp.fillRect(x,     y + r,     w,       h-2*r, WHITE);
    _disp.fillCircle(x + r,     y + r,     r, WHITE);
    _disp.fillCircle(x + w - r, y + r,     r, WHITE);
    _disp.fillCircle(x + r,     y + h - r, r, WHITE);
    _disp.fillCircle(x + w - r, y + h - r, r, WHITE);
  }

  // ── Draw rounded rect BLACK (erase) ───────────────────────────────────────
  void _clearRR(int x, int y, int w, int h, int r) {
    if (w <= 0 || h <= 0) return;
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    _disp.fillRect(x + r, y,         w - 2*r, h,     BLACK);
    _disp.fillRect(x,     y + r,     w,       h-2*r, BLACK);
    _disp.fillCircle(x + r,     y + r,     r, BLACK);
    _disp.fillCircle(x + w - r, y + r,     r, BLACK);
    _disp.fillCircle(x + r,     y + h - r, r, BLACK);
    _disp.fillCircle(x + w - r, y + h - r, r, BLACK);
  }

  // ── Draw triangle ─────────────────────────────────────────────────────────
  void _fillTri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t col) {
    _disp.fillTriangle(x0, y0, x1, y1, x2, y2, col);
  }

  // ── Main draw ─────────────────────────────────────────────────────────────
  void _draw() {
    const MoodDef& m = MOODS[_expr];

    _disp.clearDisplay();

    // ── Base eye positions ─────────────────────────────────────────────────
    int totalW = (int)(_eyeW + _space + _eyeW);
    int baseX  = (ZE_W - totalW) / 2 + (int)_LX;
    int baseY  = (ZE_H - (int)_eyeH) / 2 + (int)_LY;

    // Blink — reduce height
    float blinkClose = fmaxf(_blinkH, 0);
    int drawH = fmaxf(1, _eyeH * (1.0f - blinkClose));

    int lx = baseX;
    int ly = baseY + ((int)_eyeH - drawH) / 2;
    int rx = lx + (int)_eyeW + (int)_space;
    int ry = ly;
    int ew = (int)_eyeW;
    int er = (int)_eyeR;

    // ── 1. BASE EYES ───────────────────────────────────────────────────────
    if (!m.excited) {
      _fillRR(lx, ly, ew, drawH, er);
      _fillRR(rx, ry, ew, drawH, er);
    }

    // ── TIRED ──────────────────────────────────────────────────────────────
    if (_tiredH > 0.5f) {
      float sliver  = drawH * 0.04f;
      float quarter = drawH * 0.28f;
      float visible;
      if (_tiredPulseDir == 0) {
        visible = drawH * (1 - _tiredPulse) + sliver * _tiredPulse;
      } else if (_tiredPulseDir == 1) {
        visible = sliver + (quarter - sliver) * _tiredPulse;
      } else {
        visible = quarter - (quarter - sliver) * _tiredPulse;
      }
      int cut = fmaxf(0, drawH - visible);
      _disp.fillRect(lx-1, ly-1, ew+2, cut+1, BLACK);
      _disp.fillRect(rx-1, ry-1, ew+2, cut+1, BLACK);
    }

    // ── SAD ────────────────────────────────────────────────────────────────
    if (_sadH > 0.5f) {
      float prog   = fminf(1.0f, _sadH / (_eyeH / 2.0f));
      int   dropY  = (int)(prog * 10);
      int   squint = (int)(prog * _eyeH * 0.62f);
      _disp.fillRect(lx-2, ly-2, ew+4, drawH+14, BLACK);
      _disp.fillRect(rx-2, ry-2, ew+4, drawH+14, BLACK);
      _fillRR(lx, ly + dropY, ew, drawH, er);
      _fillRR(rx, ry + dropY, ew, drawH, er);
      _fillTri(lx, ly+dropY-1, lx+ew, ly+dropY-1, lx, ly+dropY+squint-1, BLACK);
      _fillTri(rx, ry+dropY-1, rx+ew, ry+dropY-1, rx+ew, ry+dropY+squint-1, BLACK);
      // Sad brows
      int lly = ly + dropY + squint*4/10;
      int rry = ry + dropY + squint*4/10;
      _disp.drawLine(lx+ew, lly - squint/2 - 4, lx, lly - 2, WHITE);
      _disp.drawLine(rx, rry - squint/2 - 4, rx+ew, rry - 2, WHITE);
    }

    // ── ANGRY ──────────────────────────────────────────────────────────────
    if (_angryH > 0.5f) {
      float ep   = _moodProg;
      float shrink = 0.88f;
      int   aW   = (int)(_eyeW * (1.0f - (1.0f - shrink) * ep));
      int   aH   = (int)(drawH  * (1.0f - (1.0f - shrink) * ep));
      int   aR   = (int)(er + (aW/2 - er) * ep);
      int   alx  = lx + (ew - aW)/2;
      int   aly  = ly + (drawH - aH)/2 + (int)(aH*0.1f);
      int   arx  = rx + (ew - aW)/2;
      int   ary  = ry + (drawH - aH)/2 + (int)(aH*0.1f);
      _disp.fillRect(lx-2, ly-2, ew+4, drawH+4, BLACK);
      _disp.fillRect(rx-2, ry-2, ew+4, drawH+4, BLACK);
      _fillRR(alx, aly, aW, aH, aR);
      _fillRR(arx, ary, aW, aH, aR);
      // Angry brows
      _disp.drawLine(alx,      aly - 13, alx+aW+4, aly - 13 + (int)(aH*0.15f), WHITE);
      _disp.drawLine(arx+aW,   ary - 13, arx - 4,  ary - 13 + (int)(aH*0.15f), WHITE);
    }

    // ── CONFUSED ───────────────────────────────────────────────────────────
    if (m.confused) {
      _disp.fillRect(lx-10, ly-20, ew+20, drawH+34, BLACK);
      _disp.fillRect(rx-10, ry-20, ew+20, drawH+34, BLACK);
      int bw = (int)(_eyeW * 1.1f);
      int bh = (int)(drawH * 1.1f);
      int blx = lx - (bw - ew)/2;
      int brx = rx - (bw - ew)/2;
      int bly = ly - (bh - drawH)/2;
      int bry = ry - (bh - drawH)/2;
      _fillRR(blx, bly, bw, bh, bw*45/100);
      _fillRR(brx, bry, bw, bh, bw*45/100);
      // Confused brows — left slopes right-down, right raised higher
      _disp.drawLine(blx + bw*12/100, bly - 13 - 2,
                     blx + bw*12/100 + bw*75/100, bly - 13 + 6, WHITE);
      _disp.drawLine(brx + bw*2/100, bry - 22 + 8,
                     brx + bw*2/100 + (int)(bw*1.1f)*75/100, bry - 22 - 1, WHITE);
    }

    // ── CURIOUS ────────────────────────────────────────────────────────────
    if (m.curious) {
      _clearRR(lx-2, ly-2, ew+4, drawH+4, er+2);
      int smW = (int)(_eyeW * 0.82f);
      int smH = (int)(drawH * 0.82f);
      _fillRR(lx + (ew-smW)/2, ly + (drawH-smH)/2, smW, smH, er);
      _clearRR(rx-6, ry-6, ew+12, drawH+12, er+3);
      int bigW = (int)(_eyeW * 1.15f);
      int bigH = (int)(drawH * 1.15f);
      int bigX = rx - (bigW - ew)/2;
      int bigY = ry - (bigH - drawH)/2;
      _fillRR(bigX, bigY, bigW, bigH, er+3);
      // Magnifying glass ring
      int mgCX = rx + ew/2;
      int mgCY = ry + drawH/2;
      int mgR  = (int)((fmaxf(bigW, bigH)/2.0f + 7) * _moodProg);
      if (mgR > 2) {
        _disp.drawCircle(mgCX, mgCY, mgR, WHITE);
        // Handle toward lower-right
        float ha = 3.14159f * 0.22f;
        int hx1 = mgCX + (int)(cosf(ha) * mgR);
        int hy1 = mgCY + (int)(sinf(ha) * mgR);
        int hx2 = hx1  + (int)(cosf(ha) * 22);
        int hy2 = hy1  + (int)(sinf(ha) * 22);
        _disp.drawLine(hx1, hy1, hx2, hy2, WHITE);
      }
      // Curious brows
      _disp.drawLine(lx + (ew-smW)/2, ly - 4,
                     lx + (ew-smW)/2 + smW, ly - 4, WHITE);
      _disp.drawLine(bigX, bigY - 13, bigX + bigW, bigY - 13, WHITE);
    }

    // ── EXCITED ────────────────────────────────────────────────────────────
    if (m.excited) {
      int bXL   = (ZE_W - (int)(_eyeW + _space + _eyeW)) / 2;
      int bXR   = bXL + (int)_eyeW + (int)_space;
      int bYC   = (ZE_H - (int)_eyeH) / 2;
      float lScale = _excitedLeftLeads ? 1.22f : 0.78f;
      float rScale = _excitedLeftLeads ? 0.78f : 1.22f;
      int lW = (int)(_eyeW * lScale), lH = (int)(_eyeH * lScale);
      int rW = (int)(_eyeW * rScale), rH = (int)(_eyeH * rScale);
      int lExX = bXL + (int)_excitedRoamLX - (lW - (int)_eyeW)/2;
      int lExY = bYC + (int)_excitedRoamLY - (lH - (int)_eyeH)/2;
      int rExX = bXR + (int)_excitedRoamRX - (rW - (int)_eyeW)/2;
      int rExY = bYC + (int)_excitedRoamRY - (rH - (int)_eyeH)/2;
      _fillRR(lExX, lExY, lW, lH, (int)(er * lScale));
      _fillRR(rExX, rExY, rW, rH, (int)(er * rScale));
    }

    // ── BORED/SARCASTIC ────────────────────────────────────────────────────
    if (m.sarcastic) {
      int sh = (int)(drawH * 0.62f);
      _fillTri(lx+ew/2, ly-1, lx+ew, ly-1, lx+ew, ly+sh-1, BLACK);
      _fillTri(rx,       ry-1, rx+ew/2, ry-1, rx,   ry+sh-1, BLACK);
      _disp.fillRect(lx-1, ly-1, ew+2, (int)(drawH*0.6f), BLACK);
      _disp.fillRect(rx-1, ry-1, ew+2, (int)(drawH*0.6f), BLACK);
      int stripH = fmaxf(1, (int)(drawH * 0.18f));
      _fillRR(lx+1, ly+drawH-stripH-1, ew-2, stripH, 2);
      _fillRR(rx+1, ry+drawH-stripH-1, ew-2, stripH, 2);
      // Flat brows
      _disp.drawLine(lx, ly-7, lx+ew, ly-5, WHITE);
      _disp.drawLine(rx, ry-5, rx+ew, ry-7, WHITE);
    }

    // ── SURPRISED ──────────────────────────────────────────────────────────
    if (m.surprised && _surprisedScale > 0.01f) {
      float sc  = fmaxf(0.05f, _surprisedScale);
      int   cx1 = lx + ew/2;
      int   cy1 = ly + drawH/2;
      int   cx2 = rx + ew/2;
      int   cy2 = ry + drawH/2;
      int   sw  = (int)(_eyeW * sc);
      int   sh2 = (int)(drawH * sc);
      int   sr  = fminf(sw, sh2) / 2;
      _disp.fillRect(lx-ew/2, ly-drawH/2, ew*2, drawH*2, BLACK);
      _disp.fillRect(rx-ew/2, ry-drawH/2, ew*2, drawH*2, BLACK);
      _fillRR(cx1 - sw/2, cy1 - sh2/2, sw, sh2, sr);
      _fillRR(cx2 - sw/2, cy2 - sh2/2, sw, sh2, sr);
      // Brows shot up high
      int browLift = sh2/2 + 18;
      _disp.drawLine(cx1 - sw*55/100, cy1 - browLift,
                     cx1 + sw*55/100, cy1 - browLift, WHITE);
      _disp.drawLine(cx2 - sw*55/100, cy2 - browLift,
                     cx2 + sw*55/100, cy2 - browLift, WHITE);
    }

    // ── HAPPY arch (cut bottom) ────────────────────────────────────────────
    if (_happyOff > 0.5f) {
      _disp.fillRect(lx-1, (ly+drawH)-(int)_happyOff+1, ew+2, (int)_eyeH+4, BLACK);
      _disp.fillRect(rx-1, (ry+drawH)-(int)_happyOff+1, ew+2, (int)_eyeH+4, BLACK);
    }

    // ── LOVE hearts ────────────────────────────────────────────────────────
    if (m.love && _happyOff > _eyeH*0.25f) {
      int cx1 = lx + ew/2;
      int cx2 = rx + ew/2;
      int cyH = ly + (int)(drawH * 0.3f);
      int s   = (int)(_eyeW * 0.3f);
      // Simple heart approximation using filled circles + triangle
      for (int hci = 0; hci < 2; hci++) {
        int cx = (hci == 0) ? cx1 : cx2;
        _disp.fillCircle(cx - s/2, cyH, s/2, WHITE);
        _disp.fillCircle(cx + s/2, cyH, s/2, WHITE);
        _fillTri(cx - s, cyH, cx + s, cyH, cx, cyH + s, WHITE);
      }
    }

    // ── SHY blush dots ────────────────────────────────────────────────────
    if (m.shy) {
      int blushY = ly + drawH + 6;
      for (int i = 0; i < 3; i++) {
        _disp.fillCircle(lx + ew*2/10 + i*6, blushY, 3-i, WHITE);
        _disp.fillCircle(rx + ew*2/10 + i*6, blushY, 3-i, WHITE);
      }
    }

    // ── SLEEP — closed lids + Zzz ─────────────────────────────────────────
    if (_sleepOff > 0.5f) {
      _disp.fillRect(lx-1, ly-1, ew+2, (int)_sleepOff + er + 2, BLACK);
      _disp.fillRect(rx-1, ry-1, ew+2, (int)_sleepOff + er + 2, BLACK);
      // Droopy brow lines
      _disp.drawLine(lx+8, ZE_H/2+2, lx+ew-4+8, ZE_H/2-2, WHITE);
      _disp.drawLine(rx+8, ZE_H/2+2, rx+ew-4+8, ZE_H/2-2, WHITE);
      // Z and z floating up
      int zyo = (int)((fmodf(_sleepBreath * 28.0f, 75.0f) / 75.0f) * 16);
      _disp.setTextSize(1);
      _disp.setTextColor(WHITE);
      _disp.setCursor(rx + ew + 4, ry - 10 - zyo);
      _disp.print('z');
      _disp.setCursor(rx + ew + 11, ry - 14 - zyo);
      _disp.print('Z');
    }

    // ── ANXIOUS trembling brows ────────────────────────────────────────────
    if (m.anxious) {
      int bShake = (int)(sinf(_anxiousBrowShake) * 1.5f);
      _disp.drawLine(lx,    ly - 14 + bShake, lx + ew, ly - 4 + bShake, WHITE);
      _disp.drawLine(rx+ew, ry - 14 + bShake, rx,      ry - 4 + bShake, WHITE);
    }

    // ── DIZZY spinning X eyes ─────────────────────────────────────────────
    if (m.dizzy) {
      int eyes[2][2] = { {lx + ew/2, ly + drawH/2}, {rx + ew/2, ry + drawH/2} };
      for (int i = 0; i < 2; i++) {
        int cx = eyes[i][0];
        int cy = eyes[i][1] + (int)(sinf(_dizzyAngle*1.5f + i*2)*3);
        int s  = (int)(_eyeW * 0.36f);
        _disp.fillCircle(cx, cy, s, BLACK);
        // Orbit dot
        float oa = _dizzyAngle * 2.5f + i * 3.14159f;
        int dotX = cx + (int)(cosf(oa) * s * 0.55f);
        int dotY = cy + (int)(sinf(oa) * s * 0.55f);
        _disp.fillCircle(dotX, dotY, fmaxf(1, (int)(s*0.26f*0.9f)), WHITE);
        // X lines
        float a1 = _dizzyAngle * 0.8f + 3.14159f*0.25f;
        float a2 = _dizzyAngle * 0.8f - 3.14159f*0.25f;
        _disp.drawLine(cx + (int)(cosf(a1)*s*0.82f), cy + (int)(sinf(a1)*s*0.82f),
                       cx - (int)(cosf(a1)*s*0.82f), cy - (int)(sinf(a1)*s*0.82f), WHITE);
        _disp.drawLine(cx + (int)(cosf(a2)*s*0.82f), cy + (int)(sinf(a2)*s*0.82f),
                       cx - (int)(cosf(a2)*s*0.82f), cy - (int)(sinf(a2)*s*0.82f), WHITE);
      }
      // Stars orbiting
      for (int i = 0; i < 3; i++) {
        float angle = _dizzyAngle * 1.8f + i * (3.14159f * 2.0f / 3.0f);
        int starX = ZE_W/2 + (int)(cosf(angle) * (_eyeW + _space*0.9f));
        int starY = ZE_H/2 + (int)(sinf(angle) * 16);
        _disp.drawPixel(starX, starY, WHITE);
        _disp.drawPixel(starX+1, starY, WHITE);
      }
    }

    // ── GLITCH scanline flicker ────────────────────────────────────────────
    if (_glitchActive) {
      for (int g = 0; g < 6; g++) {
        int gx = random(0, ZE_W);
        int gy = random(0, ZE_H);
        int gw = 2 + random(0, 18);
        _disp.drawFastHLine(gx, gy, gw, WHITE);
        _disp.drawFastHLine(gx+1, gy, gw, BLACK);
      }
    }

    _disp.display();
  }
};

// ── MOODS table ──────────────────────────────────────────────────────────────
const ZarEyes::MoodDef ZarEyes::MOODS[ZarEyes::EXPR_COUNT] = {
  // eyeW eyeH  r  space  tired  angry  happy  sleep  sad    surp   anx    love   conf   excit  sarc   shy    dizzy
  { 46,  46,  10,  18,  false, false, false, false, false, false, false, false, false, false, false, false, false }, // DEFAULT
  { 46,  46,  10,  18,  false, false, true,  false, false, false, false, false, false, false, false, false, false }, // HAPPY
  { 44,  44,  10,  18,  false, false, false, false, true,  false, false, false, false, false, false, false, false }, // SAD
  { 46,  46,  23,  22,  false, true,  false, false, false, false, false, false, false, false, false, false, false }, // ANGRY
  { 40,  48,  20,  28,  false, false, false, false, false, false, false, false, false, false, false, false, false }, // CURIOUS  (curious handled by flag check)
  { 46,  40,  10,  18,  true,  false, false, false, false, false, false, false, false, false, false, false, false }, // TIRED
  { 54,  54,  27,  20,  false, false, false, false, false, true,  false, false, false, false, false, false, false }, // SURPRISED
  { 44,  48,  12,  20,  false, false, false, false, false, false, true,  false, false, false, false, false, false }, // ANXIOUS
  { 46,  46,  12,  20,  false, false, true,  false, false, false, false, true,  false, false, false, false, false }, // LOVE
  { 38,  38,   9,  18,  false, false, false, false, false, false, false, false, true,  false, false, false, false }, // CONFUSED
  { 46,  50,  12,   6,  false, false, false, false, false, false, false, false, false, true,  false, false, false }, // EXCITED
  { 48,  40,   6,  18,  false, false, false, false, false, false, false, false, false, false, true,  false, false }, // BORED
  { 34,  34,   9,  10,  false, false, true,  false, false, false, false, false, false, false, false, true,  false }, // SHY
  { 46,  46,  10,  18,  false, false, false, true,  false, false, false, false, false, false, false, false, false }, // SLEEP
  { 44,  44,  10,  18,  false, false, false, false, false, false, false, false, false, false, false, false, true  }, // DIZZY
};
