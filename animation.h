#pragma once
#include "timer.h"

/**
 * @brief Animation uses a Timer and frameCount to determine which frame should be drawn in the current render loop.
 *
 */
class Animation
{
  Timer timer;
  int frameCount;

  public:
    Animation() : timer(0), frameCount(0) {}
    Animation(int frameCount, float length) : frameCount(frameCount), timer(length) {}

    float getLength() const { return timer.getLength(); }

    // we take a step every frame
    void step(float deltaTime) {
      timer.step(deltaTime);
      return;
    };

    /**
     * @brief currentFrame determines which frame we should draw by checking the timer, since the timer is updated every render loop, we can determine which frame should be rendered based on how far along the total animation time length we are at. Eg. if we are 0.40% done with the timer length, and we have 4 total frames, we will draw frame 0.4 * 4 = 1.6 -> cast to int truncates towards 0 -> frame 1
     */
    int currentFrame() const {
      float progress = timer.getTime() / timer.getLength();      // 0..1
      int frame = static_cast<int>(progress * frameCount);       // 0..frameCount-1
      return frame;
    };

    bool isDone() const { return timer.isTimedOut(); }

};