#include "CustomEyeEngine.h"

CustomEyeEngine::CustomEyeEngine(Adafruit_SSD1306* displayPtr) : _display(displayPtr) {
    _screenWidth = displayPtr->width();
    _screenHeight = displayPtr->height();

    EyeParameters defaultParams = {
        32, 32, 25, 20, 8, 0, 0, 8, 0, 0,
        14, 0, 0, 4, 2, -2, 0, 0, 0,
        false, false, false, false, false, false
    };

    _baseMoodParams = defaultParams;

    _leftEyeState.startParams = defaultParams;
    _leftEyeState.currentParams = defaultParams;
    _leftEyeState.targetParams = defaultParams;
    _leftEyeState.startTime = 0;
    _leftEyeState.duration = 0;
    _leftEyeState.animating = false;

    _rightEyeState.startParams = defaultParams;
    _rightEyeState.startParams.centerX = _screenWidth - defaultParams.centerX;
    _rightEyeState.currentParams = _rightEyeState.startParams;
    _rightEyeState.targetParams = _rightEyeState.startParams;
    _rightEyeState.startTime = 0;
    _rightEyeState.duration = 0;
    _rightEyeState.animating = false;
}

void CustomEyeEngine::begin() {
    _display->clearDisplay();
    drawEye(_leftEyeState.currentParams, true);
    drawEye(_rightEyeState.currentParams, false);
    _display->display();
}

void CustomEyeEngine::update() {
    const unsigned long currentTime = millis();

    if (_autoblinker && currentTime >= _nextBlinkTime) {
        blink(150);
        _nextBlinkTime = currentTime + (_blinkInterval * 1000UL) + random(_blinkVariation * 1000UL);
    }

    if (_idleMode && currentTime >= _nextIdleTime) {
        int tx = random(0, _screenWidth);
        int ty = random(0, _screenHeight);
        lookAt(tx, ty, 300);
        _nextIdleTime = currentTime + (_idleInterval * 1000UL) + random(_idleVariation * 1000UL);
    }

    if (_blinkActive) {
        unsigned long phaseDuration = _blinkDuration / 2;
        if (phaseDuration == 0) phaseDuration = 1;

        if (_blinkClosing && (currentTime - _blinkPhaseStart) >= phaseDuration) {
            EyeParameters openTarget = _baseMoodParams;
            openTarget.pupilOffsetX += _gazeOffsetX;
            openTarget.pupilOffsetY += _gazeOffsetY;
            openTarget.irisOffsetX += _gazeOffsetX;
            openTarget.irisOffsetY += _gazeOffsetY;
            setCustomParams(openTarget, phaseDuration);
            _blinkClosing = false;
            _blinkPhaseStart = currentTime;
        } else if (!_blinkClosing && !_leftEyeState.animating && !_rightEyeState.animating) {
            _blinkActive = false;
        }
    }

    if (_confused && currentTime >= _confusedTimer + _confusedDuration) _confused = false;
    if (_laughing && currentTime >= _laughingTimer + _laughingDuration) _laughing = false;
    if (_dizzy && currentTime >= _dizzyTimer + _dizzyDuration) _dizzy = false;

    interpolateEyeParams(_leftEyeState);
    interpolateEyeParams(_rightEyeState);

    bool needsUpdate = _leftEyeState.animating || _rightEyeState.animating ||
                       !isAtTarget(_leftEyeState) || !isAtTarget(_rightEyeState);

    if (needsUpdate || _confused || _laughing || _dizzy) {
        _display->clearDisplay();

        EyeParameters l = _leftEyeState.currentParams;
        EyeParameters r = _rightEyeState.currentParams;

        if (_confused) {
            int off = random(-2, 3);
            l.centerX += off;
            r.centerX += off;
        }
        if (_laughing) {
            int off = random(-2, 3);
            l.centerY += off;
            r.centerY += off;
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
    _baseMoodParams = getParamsForMood(newMood);
    _baseMoodParams.pupilOffsetX += _gazeOffsetX;
    _baseMoodParams.pupilOffsetY += _gazeOffsetY;
    _baseMoodParams.irisOffsetX += _gazeOffsetX;
    _baseMoodParams.irisOffsetY += _gazeOffsetY;

    _blinkActive = false;
    _blinkClosing = false;

    setCustomParams(_baseMoodParams, duration);
}

void CustomEyeEngine::setCustomParams(const EyeParameters& params, unsigned long animDuration) {
    EyeParameters leftTarget = params;
    leftTarget.width = clampInt(leftTarget.width, 2, _screenWidth);
    leftTarget.height = clampInt(leftTarget.height, 2, _screenHeight);
    leftTarget.borderRadius = clampInt(leftTarget.borderRadius, 0, leftTarget.height / 2);
    leftTarget.centerX = clampInt(leftTarget.centerX, leftTarget.width / 2, _screenWidth - leftTarget.width / 2);
    leftTarget.centerY = clampInt(leftTarget.centerY, leftTarget.height / 2, _screenHeight - leftTarget.height / 2);

    EyeParameters rightTarget = leftTarget;
    rightTarget.centerX = _screenWidth - leftTarget.centerX;

    startAnimation(_leftEyeState, leftTarget, animDuration);
    startAnimation(_rightEyeState, rightTarget, animDuration);
}

void CustomEyeEngine::blink(unsigned long duration) {
    EyeParameters closed = _baseMoodParams;
    closed.height = 2;
    closed.borderRadius = 1;
    closed.squintTop = 0;
    closed.squintBottom = 0;
    closed.pupilSize = 0;
    closed.irisSize = 0;
    closed.reflectionSize = 0;

    unsigned long phaseDuration = duration / 2;
    if (phaseDuration == 0) phaseDuration = 1;

    _blinkActive = true;
    _blinkClosing = true;
    _blinkDuration = duration;
    _blinkPhaseStart = millis();
    setCustomParams(closed, phaseDuration);
}

void CustomEyeEngine::lookAt(int x, int y, unsigned long duration) {
    EyeParameters target = _baseMoodParams;

    _gazeOffsetX = map(x, 0, _screenWidth, -target.width / 4, target.width / 4);
    _gazeOffsetY = map(y, 0, _screenHeight, -target.height / 4, target.height / 4);

    _gazeOffsetX = clampInt(_gazeOffsetX, -target.width / 3, target.width / 3);
    _gazeOffsetY = clampInt(_gazeOffsetY, -target.height / 3, target.height / 3);

    target.pupilOffsetX += _gazeOffsetX;
    target.pupilOffsetY += _gazeOffsetY;
    target.irisOffsetX += _gazeOffsetX;
    target.irisOffsetY += _gazeOffsetY;
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
        case NEUTRAL:
        default:         return {32, 32, 25, 25, 8, 0, 0, 8, 0, 0, 14, 0, 0, 4, 2, -2, 0, 0, 0, false, false, false, false, false, false};
    }
}

void CustomEyeEngine::drawEye(const EyeParameters& params, bool isLeftEye) {
    _display->fillRoundRect(params.centerX - params.width / 2, params.centerY - params.height / 2,
                            params.width, params.height, params.borderRadius, SSD1306_WHITE);

    if (params.irisSize > 0) drawIris(params.centerX, params.centerY, params.irisSize, params.irisOffsetX, params.irisOffsetY);
    if (params.pupilSize > 0) drawPupil(params.centerX, params.centerY, params.pupilSize, params.pupilOffsetX, params.pupilOffsetY);
    if (params.reflectionSize > 0) drawReflection(params.centerX, params.centerY, params.reflectionSize, params.reflectionOffsetX, params.reflectionOffsetY);

    drawEyelid(params, isLeftEye);
    drawEyebrow(params, isLeftEye);
    drawSpecialOverlays(params, isLeftEye);
}

void CustomEyeEngine::drawIris(int centerX, int centerY, int size, int offsetX, int offsetY) {
    _display->drawCircle(centerX + offsetX, centerY + offsetY, size / 2, SSD1306_BLACK);
}

void CustomEyeEngine::drawPupil(int centerX, int centerY, int size, int offsetX, int offsetY) {
    _display->fillCircle(centerX + offsetX, centerY + offsetY, size / 2, SSD1306_BLACK);
}

void CustomEyeEngine::drawReflection(int centerX, int centerY, int size, int offsetX, int offsetY) {
    _display->fillCircle(centerX + offsetX, centerY + offsetY, size / 2, SSD1306_WHITE);
}

void CustomEyeEngine::drawEyelid(const EyeParameters& params, bool isLeftEye) {
    if (params.squintTop > 0) {
        _display->fillRect(params.centerX - params.width / 2 - 2, params.centerY - params.height / 2 - 2,
                           params.width + 4, params.squintTop, SSD1306_BLACK);
    }
    if (params.squintBottom > 0) {
        _display->fillRect(params.centerX - params.width / 2 - 2, params.centerY + params.height / 2 - params.squintBottom + 2,
                           params.width + 4, params.squintBottom, SSD1306_BLACK);
    }
    if (params.eyelidCurve != 0) {
        int curveHeight = abs(params.eyelidCurve);
        if (params.eyelidCurve > 0) {
            _display->fillCircle(params.centerX, params.centerY + params.height / 2 + curveHeight, params.width, SSD1306_BLACK);
        } else {
            _display->fillCircle(params.centerX, params.centerY - params.height / 2 - curveHeight, params.width, SSD1306_BLACK);
        }
    }
    if (params.isAngry) {
        int x1 = params.centerX - params.width / 2;
        int y1 = params.centerY - params.height / 2;
        int x2 = params.centerX + params.width / 2;
        int y2 = y1;
        int x3 = isLeftEye ? x2 : x1;
        int y3 = params.centerY;
        _display->fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_BLACK);
    }
    if (params.isTired) {
        int x1 = params.centerX - params.width / 2;
        int y1 = params.centerY - params.height / 2;
        int x2 = params.centerX + params.width / 2;
        int y2 = y1;
        int x3 = isLeftEye ? x1 : x2;
        int y3 = params.centerY + params.height / 2;
        _display->fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_BLACK);
    }
}

void CustomEyeEngine::drawEyebrow(const EyeParameters& params, bool isLeftEye) {
    if (params.eyebrowHeight == 0) return;

    int x = params.centerX;
    int y = params.centerY - params.height / 2 - params.eyebrowHeight;
    int w = params.width * 0.8;
    float rad = params.eyebrowAngle * PI / 180.0;

    if (!isLeftEye) rad = -rad;

    int xOff = cos(rad) * w / 2;
    int yOff = sin(rad) * w / 2;
    _display->drawLine(x - xOff, y - yOff, x + xOff, y + yOff, SSD1306_WHITE);
}

void CustomEyeEngine::drawSpecialOverlays(const EyeParameters& params, bool isLeftEye) {
    (void)isLeftEye;
    if (params.hasHeart) {
        int x = params.centerX, y = params.centerY;
        _display->fillCircle(x - 3, y - 2, 4, SSD1306_BLACK);
        _display->fillCircle(x + 3, y - 2, 4, SSD1306_BLACK);
        _display->fillTriangle(x - 7, y - 1, x + 7, y - 1, x, y + 6, SSD1306_BLACK);
    }
    if (params.hasStar) {
        int x = params.centerX, y = params.centerY;
        _display->fillTriangle(x, y - 6, x - 2, y + 2, x + 2, y + 2, SSD1306_BLACK);
        _display->fillTriangle(x - 6, y - 2, x + 6, y - 2, x, y + 4, SSD1306_BLACK);
    }
    if (params.hasCross) {
        int x = params.centerX, y = params.centerY, s = 6;
        _display->drawLine(x - s, y - s, x + s, y + s, SSD1306_BLACK);
        _display->drawLine(x + s, y - s, x - s, y + s, SSD1306_BLACK);
    }
    if (params.isScanning) {
        int y = params.centerY - params.height / 2 + (millis() % 2000) * params.height / 2000;
        _display->drawFastHLine(params.centerX - params.width / 2, y, params.width, SSD1306_BLACK);
    }
}

void CustomEyeEngine::startAnimation(EyeAnimationState& state, const EyeParameters& target, unsigned long animDuration) {
    state.startParams = state.currentParams;
    state.targetParams = target;
    state.startTime = millis();
    state.duration = animDuration;
    state.animating = (animDuration > 0);

    if (animDuration == 0) {
        state.currentParams = target;
        state.animating = false;
    }
}

bool CustomEyeEngine::isAtTarget(const EyeAnimationState& state) const {
    const EyeParameters& c = state.currentParams;
    const EyeParameters& t = state.targetParams;

    return c.centerX == t.centerX && c.centerY == t.centerY &&
           c.width == t.width && c.height == t.height &&
           c.borderRadius == t.borderRadius &&
           c.squintTop == t.squintTop && c.squintBottom == t.squintBottom &&
           c.pupilSize == t.pupilSize && c.pupilOffsetX == t.pupilOffsetX && c.pupilOffsetY == t.pupilOffsetY &&
           c.irisSize == t.irisSize && c.irisOffsetX == t.irisOffsetX && c.irisOffsetY == t.irisOffsetY &&
           c.reflectionSize == t.reflectionSize && c.reflectionOffsetX == t.reflectionOffsetX && c.reflectionOffsetY == t.reflectionOffsetY &&
           c.eyelidCurve == t.eyelidCurve && c.eyebrowAngle == t.eyebrowAngle && c.eyebrowHeight == t.eyebrowHeight &&
           c.isAngry == t.isAngry && c.isTired == t.isTired &&
           c.hasHeart == t.hasHeart && c.hasStar == t.hasStar && c.hasCross == t.hasCross && c.isScanning == t.isScanning;
}

int CustomEyeEngine::clampInt(int value, int minValue, int maxValue) const {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

void CustomEyeEngine::interpolateEyeParams(EyeAnimationState& state) {
    if (!state.animating) return;

    unsigned long elapsed = millis() - state.startTime;
    if (elapsed >= state.duration) {
        state.currentParams = state.targetParams;
        state.animating = false;
        return;
    }

    float progress = (float)elapsed / (float)state.duration;
    progress = progress * progress * (3.0f - (2.0f * progress));

    auto& s = state.startParams;
    auto& c = state.currentParams;
    auto& t = state.targetParams;

    c.centerX = lerp(s.centerX, t.centerX, progress);
    c.centerY = lerp(s.centerY, t.centerY, progress);
    c.width = lerp(s.width, t.width, progress);
    c.height = lerp(s.height, t.height, progress);
    c.borderRadius = lerp(s.borderRadius, t.borderRadius, progress);
    c.squintTop = lerp(s.squintTop, t.squintTop, progress);
    c.squintBottom = lerp(s.squintBottom, t.squintBottom, progress);
    c.pupilSize = lerp(s.pupilSize, t.pupilSize, progress);
    c.pupilOffsetX = lerp(s.pupilOffsetX, t.pupilOffsetX, progress);
    c.pupilOffsetY = lerp(s.pupilOffsetY, t.pupilOffsetY, progress);
    c.irisSize = lerp(s.irisSize, t.irisSize, progress);
    c.irisOffsetX = lerp(s.irisOffsetX, t.irisOffsetX, progress);
    c.irisOffsetY = lerp(s.irisOffsetY, t.irisOffsetY, progress);
    c.reflectionSize = lerp(s.reflectionSize, t.reflectionSize, progress);
    c.reflectionOffsetX = lerp(s.reflectionOffsetX, t.reflectionOffsetX, progress);
    c.reflectionOffsetY = lerp(s.reflectionOffsetY, t.reflectionOffsetY, progress);
    c.eyelidCurve = lerp(s.eyelidCurve, t.eyelidCurve, progress);
    c.eyebrowAngle = lerp(s.eyebrowAngle, t.eyebrowAngle, progress);
    c.eyebrowHeight = lerp(s.eyebrowHeight, t.eyebrowHeight, progress);

    c.isAngry = t.isAngry;
    c.isTired = t.isTired;
    c.hasHeart = t.hasHeart;
    c.hasStar = t.hasStar;
    c.hasCross = t.hasCross;
    c.isScanning = t.isScanning;
}

int CustomEyeEngine::lerp(int start, int end, float progress) {
    return start + (int)((end - start) * progress);
}
