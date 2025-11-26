#pragma once
#include "timer.h"

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
      return timer.step(deltaTime);
    };

    // because the timer is updated every frame, we can determine which frame should be rendered
    int currentFrame() const {
      float progress = timer.getTime() / timer.getLength();      // 0..1
      int frame = static_cast<int>(progress * frameCount);       // 0..frameCount-1
      return frame;
    };

};