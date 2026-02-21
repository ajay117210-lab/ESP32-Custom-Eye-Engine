#include "CustomEyeEngine.h"

CustomEyeEngine::CustomEyeEngine(Adafruit_SSD1306* displayPtr) : _display(displayPtr) {
    _screenWidth = displayPtr->width();
    _screenHeight = displayPtr->height();

    // Default neutral state
    EyeParameters defaultParams = {
        32, 32, 25, 20, 8, 0, 0, 8, 0, 0, // Base
        14, 0, 0, 4, 2, -2, 0, 0, 0,      // New params
        false, false, false, false, false, false // Flags
    };
    
    _leftEyeState.currentParams = defaultParams;
    _leftEyeState.targetParams = defaultParams;
    _leftEyeState.animating = false;

    _rightEyeState.currentParams = defaultParams;
    _rightEyeState.currentParams.centerX = _screenWidth - defaultParams.centerX;
    _rightEyeState.targetParams = _rightEyeState.currentParams;
    _rightEyeState.animating = false;
}

void CustomEyeEngine::begin() {
    _display->clearDisplay();
    drawEye(_leftEyeState.currentParams, true);
    drawEye(_rightEyeState.currentParams, false);
    _display->display();
}

void CustomEyeEngine::update() {
    unsigned long currentTime = millis();
    bool needsUpdate = false;

    // 1. Handle Macro Animations
    if (_autoblinker && currentTime >= _nextBlinkTime) {
        blink(150);
        _nextBlinkTime = currentTime + (_blinkInterval * 1000) + random(_blinkVariation * 1000);
    }

    if (_idleMode && currentTime >= _nextIdleTime) {
        int tx = random(0, _screenWidth);
        int ty = random(0, _screenHeight);
        lookAt(tx, ty, 500);
        _nextIdleTime = currentTime + (_idleInterval * 1000) + random(_idleVariation * 1000);
    }

    if (_confused && currentTime >= _confusedTimer + _confusedDuration) _confused = false;
    if (_laughing && currentTime >= _laughingTimer + _laughingDuration) _laughing = false;
    if (_dizzy && currentTime >= _dizzyTimer + _dizzyDuration) _dizzy = false;

    // 2. Interpolate Animations
    if (_leftEyeState.animating) {
        interpolateEyeParams(_leftEyeState);
        if (currentTime - _leftEyeState.startTime >= _leftEyeState.duration) {
            _leftEyeState.animating = false;
            _leftEyeState.currentParams = _leftEyeState.targetParams;
        }
        needsUpdate = true;
    }

    if (_rightEyeState.animating) {
        interpolateEyeParams(_rightEyeState);
        if (currentTime - _rightEyeState.startTime >= _rightEyeState.duration) {
            _rightEyeState.animating = false;
            _rightEyeState.currentParams = _rightEyeState.targetParams;
        }
        needsUpdate = true;
    }

    // 3. Draw if needed
    if (needsUpdate || _confused || _laughing || _dizzy) {
        _display->clearDisplay();
        
        EyeParameters l = _leftEyeState.currentParams;
        EyeParameters r = _rightEyeState.currentParams;

        if (_confused) {
            int off = random(-2, 3);
            l.centerX += off; r.centerX += off;
        }
        if (_laughing) {
            int off = random(-2, 3);
            l.centerY += off; r.centerY += off;
        }
        if (_dizzy) {
            l.pupilOffsetX = cos(currentTime / 100.0) * 5;
            l.pupilOffsetY = sin(currentTime / 100.0) * 5;
            r.pupilOffsetX = cos(currentTime / 100.0 + PI) * 5;
            r.pupilOffsetY = sin(currentTime / 100.0 + PI) * 5;
        }

        drawEye(l, true);
        drawEye(r, false);
        _display->display();
    }
}

void CustomEyeEngine::setMood(Mood newMood, unsigned long duration) {
    setCustomParams(getParamsForMood(newMood), duration);
}

void CustomEyeEngine::setCustomParams(const EyeParameters& params, unsigned long animDuration) {
    unsigned long currentTime = millis();

    _leftEyeState.targetParams = params;
    _leftEyeState.startTime = currentTime;
    _leftEyeState.duration = animDuration;
    _leftEyeState.animating = true;

    _rightEyeState.targetParams = params;
    _rightEyeState.targetParams.centerX = _screenWidth - params.centerX;
    _rightEyeState.startTime = currentTime;
    _rightEyeState.duration = animDuration;
    _rightEyeState.animating = true;
}

void CustomEyeEngine::blink(unsigned long duration) {
    EyeParameters closed = _leftEyeState.currentParams;
    closed.height = 2;
    closed.squintTop = 0;
    closed.squintBottom = 0;
    closed.pupilSize = 0;
    closed.irisSize = 0;
    setCustomParams(closed, duration / 2);
}

void CustomEyeEngine::lookAt(int x, int y, unsigned long duration) {
    EyeParameters target = _leftEyeState.targetParams;
    target.pupilOffsetX = map(x, 0, _screenWidth, -target.width/4, target.width/4);
    target.pupilOffsetY = map(y, 0, _screenHeight, -target.height/4, target.height/4);
    target.irisOffsetX = target.pupilOffsetX;
    target.irisOffsetY = target.pupilOffsetY;
    setCustomParams(target, duration);
}

void CustomEyeEngine::setAutoblinker(bool active, int interval, int variation) {
    _autoblinker = active;
    _blinkInterval = interval;
    _blinkVariation = variation;
    _nextBlinkTime = millis() + 1000;
}

void CustomEyeEngine::setIdleMode(bool active, int interval, int variation) {
    _idleMode = active;
    _idleInterval = interval;
    _idleVariation = variation;
    _nextIdleTime = millis() + 1000;
}

void CustomEyeEngine::setConfused(bool active, int duration) {
    _confused = active;
    _confusedTimer = millis();
    _confusedDuration = duration;
}

void CustomEyeEngine::setLaughing(bool active, int duration) {
    _laughing = active;
    _laughingTimer = millis();
    _laughingDuration = duration;
}

void CustomEyeEngine::setDizzy(bool active, int duration) {
    _dizzy = active;
    _dizzyTimer = millis();
    _dizzyDuration = duration;
}

EyeParameters CustomEyeEngine::getParamsForMood(Mood mood) {
    // Default: centerX, centerY, width, height, borderRadius, squintTop, squintBottom, pupilSize, pupilOffsetX, pupilOffsetY, irisSize, irisOffsetX, irisOffsetY, reflectionSize, reflectionOffsetX, reflectionOffsetY, eyelidCurve, eyebrowAngle, eyebrowHeight, isAngry, isTired, hasHeart, hasStar, hasCross, isScanning
    switch (mood) {
        case HAPPY:      return {32, 32, 28, 22, 10, 0, 0, 6, 0, -2, 16, 0, -2, 4, 3, -4, 5, 10, -5, false, false, false, false, false, false};
        case ANGRY:      return {32, 32, 25, 25, 5, 0, 0, 8, 0, 2, 14, 0, 2, 3, 2, -2, 0, -20, 5, true, false, false, false, false, false};
        case TIRED:      return {32, 32, 25, 15, 5, 0, 0, 4, 0, 0, 10, 0, 0, 2, 1, -1, -3, 0, 2, false, true, false, false, false, false};
        case SLEEPY:     return {32, 32, 25, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false, false};
        case CURIOUS:    return {32, 32, 27, 27, 12, 0, 0, 7, 0, -4, 15, 0, -4, 5, 4, -5, 0, 15, -8, false, false, false, false, false, false};
        case SAD:        return {32, 32, 25, 22, 8, 0, 0, 5, 0, 3, 12, 0, 3, 3, 2, -2, -5, 20, 5, false, false, false, false, false, false};
        case SURPRISED:  return {32, 32, 30, 30, 15, 0, 0, 10, 0, 0, 20, 0, 0, 6, 5, -5, 0, 0, -10, false, false, false, false, false, false};
        case HEART_EYES: return {32, 32, 28, 25, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, true, false, false, false};
        case STAR_EYES:  return {32, 32, 28, 25, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, true, false, false};
        case DIZZY:      return {32, 32, 25, 25, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, true, false};
        case CONFIDENT:  return {32, 32, 28, 20, 10, 5, 0, 7, 0, -1, 15, 0, -1, 4, 3, -3, 0, -15, -10, false, false, false, false, false, false};
        case SNEAKY:     return {32, 32, 25, 18, 6, 8, 0, 6, 5, 0, 12, 5, 0, 3, 2, -2, 0, -10, 2, false, false, false, false, false, false};
        case QUESTIONING:return {32, 32, 26, 26, 10, 0, 0, 7, 0, -2, 14, 0, -2, 4, 2, -2, 0, 25, -12, false, false, false, false, false, false};
        case SCARED:     return {32, 32, 22, 22, 12, 0, 0, 4, 0, 0, 8, 0, 0, 2, 1, -1, 0, 30, -15, false, false, false, false, false, false};
        case ZOMBIE:     return {32, 32, 25, 25, 5, 0, 0, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false, true};
        case BATTERY_LOW:return {32, 32, 25, 10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, true, false, false, false, false};
        case CHARGING:   return {32, 32, 25, 25, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false, true};
        case WEATHER_SUN:return {32, 32, 30, 30, 15, 0, 0, 12, 0, 0, 22, 0, 0, 8, 6, -6, 0, 0, 0, false, false, false, false, false, false};
        case WEATHER_RAIN:return {32, 32, 25, 25, 8, 0, 0, 6, 0, 2, 12, 0, 2, 3, 2, -2, -2, 0, 0, false, false, false, false, false, true};
        case WEATHER_SNOW:return {32, 32, 28, 28, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, true, false, false};
        case BLUETOOTH_CONN:return {32, 32, 25, 25, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false, true};
        case FIREWORKS:  return {32, 32, 30, 30, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, true, false, true};
        default:         return {32, 32, 25, 25, 8, 0, 0, 8, 0, 0, 14, 0, 0, 4, 2, -2, 0, 0, 0, false, false, false, false, false, false};
    }
}

void CustomEyeEngine::drawEye(const EyeParameters& params, bool isLeftEye) {
    // 1. Main Eye Body
    _display->fillRoundRect(params.centerX - params.width/2, params.centerY - params.height/2, 
                             params.width, params.height, params.borderRadius, SSD1306_WHITE);
    
    // 2. Iris
    if (params.irisSize > 0) {
        drawIris(params.centerX, params.centerY, params.irisSize, params.irisOffsetX, params.irisOffsetY);
    }

    // 3. Pupil
    if (params.pupilSize > 0) {
        drawPupil(params.centerX, params.centerY, params.pupilSize, params.pupilOffsetX, params.pupilOffsetY);
    }

    // 4. Reflection
    if (params.reflectionSize > 0) {
        drawReflection(params.centerX, params.centerY, params.reflectionSize, params.reflectionOffsetX, params.reflectionOffsetY);
    }

    // 5. Expression Overlays
    drawEyelid(params, isLeftEye);
    drawEyebrow(params, isLeftEye);
    drawSpecialOverlays(params, isLeftEye);
}

void CustomEyeEngine::drawIris(int centerX, int centerY, int size, int offsetX, int offsetY) {
    // For SSD1306 (monochrome), iris can be a pattern or just a slightly smaller circle than eye
    // Here we'll draw it as a circle that might be clipped by the eye body
    _display->drawCircle(centerX + offsetX, centerY + offsetY, size / 2, SSD1306_BLACK);
}

void CustomEyeEngine::drawPupil(int centerX, int centerY, int size, int offsetX, int offsetY) {
    _display->fillCircle(centerX + offsetX, centerY + offsetY, size / 2, SSD1306_BLACK);
}

void CustomEyeEngine::drawReflection(int centerX, int centerY, int size, int offsetX, int offsetY) {
    _display->fillCircle(centerX + offsetX, centerY + offsetY, size / 2, SSD1306_WHITE);
}

void CustomEyeEngine::drawEyelid(const EyeParameters& params, bool isLeftEye) {
    // Squinting
    if (params.squintTop > 0) {
        _display->fillRect(params.centerX - params.width/2 - 2, params.centerY - params.height/2 - 2, 
                           params.width + 4, params.squintTop, SSD1306_BLACK);
    }
    if (params.squintBottom > 0) {
        _display->fillRect(params.centerX - params.width/2 - 2, params.centerY + params.height/2 - params.squintBottom + 2, 
                           params.width + 4, params.squintBottom, SSD1306_BLACK);
    }

    // Curved Eyelids (Happy/Sad)
    if (params.eyelidCurve != 0) {
        int curveHeight = abs(params.eyelidCurve);
        if (params.eyelidCurve > 0) { // Happy curve (bottom up)
            _display->fillCircle(params.centerX, params.centerY + params.height/2 + curveHeight, 
                                 params.width, SSD1306_BLACK);
        } else { // Sad curve (top down)
            _display->fillCircle(params.centerX, params.centerY - params.height/2 - curveHeight, 
                                 params.width, SSD1306_BLACK);
        }
    }

    // Special Mood Overlays
    if (params.isAngry) {
        int x1 = params.centerX - params.width/2;
        int y1 = params.centerY - params.height/2;
        int x2 = params.centerX + params.width/2;
        int y2 = y1;
        int x3 = isLeftEye ? x2 : x1;
        int y3 = params.centerY;
        _display->fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_BLACK);
    }
    
    if (params.isTired) {
        int x1 = params.centerX - params.width/2;
        int y1 = params.centerY - params.height/2;
        int x2 = params.centerX + params.width/2;
        int y2 = y1;
        int x3 = isLeftEye ? x1 : x2;
        int y3 = params.centerY + params.height/2;
        _display->fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_BLACK);
    }
}

void CustomEyeEngine::drawEyebrow(const EyeParameters& params, bool isLeftEye) {
    if (params.eyebrowHeight == 0) return;
    
    int x = params.centerX;
    int y = params.centerY - params.height/2 - params.eyebrowHeight;
    int w = params.width * 0.8;
    int h = 2;
    
    // Simple rotated line for eyebrow
    float rad = params.eyebrowAngle * PI / 180.0;
    if (!isLeftEye) rad = -rad;
    
    int xOff = cos(rad) * w / 2;
    int yOff = sin(rad) * w / 2;
    
    _display->drawLine(x - xOff, y - yOff, x + xOff, y + yOff, SSD1306_WHITE);
}

void CustomEyeEngine::drawSpecialOverlays(const EyeParameters& params, bool isLeftEye) {
    if (params.hasHeart) {
        int x = params.centerX;
        int y = params.centerY;
        _display->fillCircle(x-3, y-2, 4, SSD1306_BLACK);
        _display->fillCircle(x+3, y-2, 4, SSD1306_BLACK);
        _display->fillTriangle(x-7, y-1, x+7, y-1, x, y+6, SSD1306_BLACK);
    }
    if (params.hasStar) {
        // Simple 5-point star approximation
        int x = params.centerX;
        int y = params.centerY;
        _display->fillTriangle(x, y-6, x-2, y+2, x+2, y+2, SSD1306_BLACK);
        _display->fillTriangle(x-6, y-2, x+6, y-2, x, y+4, SSD1306_BLACK);
    }
    if (params.hasCross) {
        int x = params.centerX;
        int y = params.centerY;
        int s = 6;
        _display->drawLine(x-s, y-s, x+s, y+s, SSD1306_BLACK);
        _display->drawLine(x+s, y-s, x-s, y+s, SSD1306_BLACK);
    }
    if (params.isScanning) {
        int y = params.centerY - params.height/2 + (millis() % 2000) * params.height / 2000;
        _display->drawFastHLine(params.centerX - params.width/2, y, params.width, SSD1306_BLACK);
    }
}

void CustomEyeEngine::interpolateEyeParams(EyeAnimationState& state) {
    unsigned long elapsed = millis() - state.startTime;
    float progress = (float)elapsed / state.duration;
    if (progress > 1.0) progress = 1.0;

    auto& c = state.currentParams;
    auto& t = state.targetParams;
    
    c.centerX = lerp(c.centerX, t.centerX, progress);
    c.centerY = lerp(c.centerY, t.centerY, progress);
    c.width = lerp(c.width, t.width, progress);
    c.height = lerp(c.height, t.height, progress);
    c.borderRadius = lerp(c.borderRadius, t.borderRadius, progress);
    c.squintTop = lerp(c.squintTop, t.squintTop, progress);
    c.squintBottom = lerp(c.squintBottom, t.squintBottom, progress);
    c.pupilSize = lerp(c.pupilSize, t.pupilSize, progress);
    c.pupilOffsetX = lerp(c.pupilOffsetX, t.pupilOffsetX, progress);
    c.pupilOffsetY = lerp(c.pupilOffsetY, t.pupilOffsetY, progress);
    
    c.irisSize = lerp(c.irisSize, t.irisSize, progress);
    c.irisOffsetX = lerp(c.irisOffsetX, t.irisOffsetX, progress);
    c.irisOffsetY = lerp(c.irisOffsetY, t.irisOffsetY, progress);
    c.reflectionSize = lerp(c.reflectionSize, t.reflectionSize, progress);
    c.reflectionOffsetX = lerp(c.reflectionOffsetX, t.reflectionOffsetX, progress);
    c.reflectionOffsetY = lerp(c.reflectionOffsetY, t.reflectionOffsetY, progress);
    c.eyelidCurve = lerp(c.eyelidCurve, t.eyelidCurve, progress);
    c.eyebrowAngle = lerp(c.eyebrowAngle, t.eyebrowAngle, progress);
    c.eyebrowHeight = lerp(c.eyebrowHeight, t.eyebrowHeight, progress);

    if (progress >= 1.0) {
        c.isAngry = t.isAngry;
        c.isTired = t.isTired;
        c.hasHeart = t.hasHeart;
        c.hasStar = t.hasStar;
        c.hasCross = t.hasCross;
        c.isScanning = t.isScanning;
    }
}

int CustomEyeEngine::lerp(int start, int end, float progress) {
    return start + (int)((end - start) * progress);
}
