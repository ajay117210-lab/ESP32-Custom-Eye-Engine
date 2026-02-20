#include "CustomEyeEngine.h"

CustomEyeEngine::CustomEyeEngine(Adafruit_SSD1306* displayPtr) : _display(displayPtr) {
    _screenWidth = displayPtr->width();
    _screenHeight = displayPtr->height();

    // Default neutral state
    EyeParameters defaultParams = {32, 32, 25, 20, 0, 0, 8, 0, 0};
    
    _leftEyeState.currentParams = defaultParams;
    _leftEyeState.targetParams = defaultParams;
    _leftEyeState.animating = false;

    _rightEyeState.currentParams = defaultParams;
    _rightEyeState.currentParams.centerX = _screenWidth - defaultParams.centerX;
    _rightEyeState.targetParams = _rightEyeState.currentParams;
    _rightEyeState.animating = false;
}

void CustomEyeEngine::begin() {
    // Note: display->begin() should be called in main sketch setup
    _display->clearDisplay();
    drawEye(_leftEyeState.currentParams, true);
    drawEye(_rightEyeState.currentParams, false);
    _display->display();
}

void CustomEyeEngine::update() {
    unsigned long currentTime = millis();
    bool needsUpdate = false;

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

    if (needsUpdate) {
        _display->clearDisplay();
        drawEye(_leftEyeState.currentParams, true);
        drawEye(_rightEyeState.currentParams, false);
        _display->display();
    }
}

void CustomEyeEngine::setMood(Mood newMood, unsigned long duration) {
    EyeParameters target = getParamsForMood(newMood);
    setCustomParams(target, duration);
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
    closed.height = 0;
    closed.squintTop = 0;
    closed.squintBottom = 0;
    
    // Quick closure
    setCustomParams(closed, duration / 2);
    // Note: For a full blink, you'd usually queue the reopening. 
    // This simple version will rely on the next setMood or manual reopen.
}

void CustomEyeEngine::lookAt(int x, int y, unsigned long duration) {
    EyeParameters target = _leftEyeState.targetParams;
    target.pupilOffsetX = map(x, 0, _screenWidth, -target.width/3, target.width/3);
    target.pupilOffsetY = map(y, 0, _screenHeight, -target.height/3, target.height/3);
    setCustomParams(target, duration);
}

EyeParameters CustomEyeEngine::getParamsForMood(Mood mood) {
    switch (mood) {
        case HAPPY:   return {32, 32, 28, 18, -5, 5, 7, 0, -2};
        case ANGRY:   return {32, 32, 25, 20, 8, -2, 9, 0, 2};
        case TIRED:   return {32, 32, 25, 10, 12, 8, 6, 0, 0};
        case CURIOUS: return {32, 32, 27, 22, -2, 0, 8, 0, -5};
        case ANXIOUS: return {32, 32, 22, 22, 2, 2, 10, 0, 0};
        case PLAYFUL: return {32, 32, 26, 21, -3, 3, 7, 3, -3};
        default:      return {32, 32, 25, 20, 0, 0, 8, 0, 0};
    }
}

void CustomEyeEngine::drawEye(const EyeParameters& params, bool isLeftEye) {
    // Base eye shape
    _display->fillEllipse(params.centerX, params.centerY, params.width, params.height, SSD1306_WHITE);
    
    // Pupil
    drawPupil(params.centerX, params.centerY, params.pupilSize, params.pupilOffsetX, params.pupilOffsetY);

    // Eyelids for squinting/expressions
    if (params.squintTop != 0) drawEyelid(params.centerX, params.centerY, params.width, params.height, params.squintTop, true);
    if (params.squintBottom != 0) drawEyelid(params.centerX, params.centerY, params.width, params.height, params.squintBottom, false);
}

void CustomEyeEngine::drawEyelid(int centerX, int centerY, int width, int height, int offset, bool isTop) {
    int ew = width + 4;
    int eh = abs(offset);
    int yPos = isTop ? (centerY - height - 2) : (centerY + height - eh + 2);
    _display->fillRect(centerX - ew/2, yPos, ew, eh, SSD1306_BLACK);
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
    c.squintTop = lerp(c.squintTop, t.squintTop, progress);
    c.squintBottom = lerp(c.squintBottom, t.squintBottom, progress);
    c.pupilSize = lerp(c.pupilSize, t.pupilSize, progress);
    c.pupilOffsetX = lerp(c.pupilOffsetX, t.pupilOffsetX, progress);
    c.pupilOffsetY = lerp(c.pupilOffsetY, t.pupilOffsetY, progress);
}

int CustomEyeEngine::lerp(int start, int end, float progress) {
    return start + (int)((end - start) * progress);
}
