#include "CustomEyeEngine.h"

CustomEyeEngine::CustomEyeEngine(Adafruit_SSD1306* displayPtr) : _display(displayPtr) {
    _screenWidth = displayPtr->width();
    _screenHeight = displayPtr->height();

    // Default neutral state
    EyeParameters defaultParams = {32, 32, 25, 20, 8, 0, 0, 8, 0, 0, false, false};
    
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

    // 1. Handle Macro Animations (Blinker, Idle, Shiver)
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

    // Shiver effects
    if (_confused && currentTime >= _confusedTimer + _confusedDuration) _confused = false;
    if (_laughing && currentTime >= _laughingTimer + _laughingDuration) _laughing = false;

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
    if (needsUpdate || _confused || _laughing) {
        _display->clearDisplay();
        
        EyeParameters l = _leftEyeState.currentParams;
        EyeParameters r = _rightEyeState.currentParams;

        // Apply shiver offsets
        if (_confused) {
            int off = random(-3, 4);
            l.centerX += off; r.centerX += off;
        }
        if (_laughing) {
            int off = random(-3, 4);
            l.centerY += off; r.centerY += off;
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
    closed.height = 1; // Almost closed
    closed.squintTop = 0;
    closed.squintBottom = 0;
    setCustomParams(closed, duration / 2);
    // Note: To fully blink and return, you'd usually use a state or timer.
    // This simple version will "squint" down.
}

void CustomEyeEngine::lookAt(int x, int y, unsigned long duration) {
    EyeParameters target = _leftEyeState.targetParams;
    target.pupilOffsetX = map(x, 0, _screenWidth, -target.width/3, target.width/3);
    target.pupilOffsetY = map(y, 0, _screenHeight, -target.height/3, target.height/3);
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

EyeParameters CustomEyeEngine::getParamsForMood(Mood mood) {
    // Default: centerX, centerY, width, height, borderRadius, squintTop, squintBottom, pupilSize, pupilOffsetX, pupilOffsetY, isAngry, isTired
    switch (mood) {
        case HAPPY:   return {32, 32, 28, 22, 10, -5, 10, 7, 0, -2, false, false};
        case ANGRY:   return {32, 32, 25, 25, 5, 0, 0, 9, 0, 2, true, false};
        case TIRED:   return {32, 32, 25, 15, 5, 0, 0, 6, 0, 0, false, true};
        case SLEEPY:  return {32, 32, 25, 5, 2, 0, 0, 0, 0, 0, false, false};
        case CURIOUS: return {32, 32, 27, 27, 12, -2, 0, 8, 0, -5, false, false};
        default:      return {32, 32, 25, 25, 8, 0, 0, 8, 0, 0, false, false};
    }
}

void CustomEyeEngine::drawEye(const EyeParameters& params, bool isLeftEye) {
    // Main Eye Body (Rounded Rect like RoboEyes)
    _display->fillRoundRect(params.centerX - params.width/2, params.centerY - params.height/2, 
                             params.width, params.height, params.borderRadius, SSD1306_WHITE);
    
    // Pupil
    if (params.pupilSize > 0) {
        drawPupil(params.centerX, params.centerY, params.pupilSize, params.pupilOffsetX, params.pupilOffsetY);
    }

    // Expression Overlays (Eyelids)
    drawEyelid(params, isLeftEye);
}

void CustomEyeEngine::drawEyelid(const EyeParameters& params, bool isLeftEye) {
    // 1. Procedural Squinting (Rectangular overlays)
    if (params.squintTop > 0) {
        _display->fillRect(params.centerX - params.width/2 - 2, params.centerY - params.height/2 - 2, 
                           params.width + 4, params.squintTop, SSD1306_BLACK);
    }
    if (params.squintBottom > 0) {
        _display->fillRect(params.centerX - params.width/2 - 2, params.centerY + params.height/2 - params.squintBottom + 2, 
                           params.width + 4, params.squintBottom, SSD1306_BLACK);
    }

    // 2. Special Mood Overlays (Triangles like RoboEyes)
    if (params.isAngry) {
        if (isLeftEye) {
            _display->fillTriangle(params.centerX - params.width/2, params.centerY - params.height/2,
                                   params.centerX + params.width/2, params.centerY - params.height/2,
                                   params.centerX + params.width/2, params.centerY, SSD1306_BLACK);
        } else {
            _display->fillTriangle(params.centerX - params.width/2, params.centerY - params.height/2,
                                   params.centerX + params.width/2, params.centerY - params.height/2,
                                   params.centerX - params.width/2, params.centerY, SSD1306_BLACK);
        }
    }
    
    if (params.isTired) {
        if (isLeftEye) {
            _display->fillTriangle(params.centerX - params.width/2, params.centerY - params.height/2,
                                   params.centerX + params.width/2, params.centerY - params.height/2,
                                   params.centerX - params.width/2, params.centerY + params.height/2, SSD1306_BLACK);
        } else {
            _display->fillTriangle(params.centerX - params.width/2, params.centerY - params.height/2,
                                   params.centerX + params.width/2, params.centerY - params.height/2,
                                   params.centerX + params.width/2, params.centerY + params.height/2, SSD1306_BLACK);
        }
    }
}

void CustomEyeEngine::drawPupil(int centerX, int centerY, int size, int offsetX, int offsetY) {
    _display->fillCircle(centerX + offsetX, centerY + offsetY, size / 2, SSD1306_BLACK);
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
    // Boolean flags don't interpolate
    if (progress >= 1.0) {
        c.isAngry = t.isAngry;
        c.isTired = t.isTired;
    }
}

int CustomEyeEngine::lerp(int start, int end, float progress) {
    return start + (int)((end - start) * progress);
}
