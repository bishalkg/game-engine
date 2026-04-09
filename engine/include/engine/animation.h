#pragma once
#include "engine/timer.h"

/**
 * @brief Animation uses a Timer and frameCount to determine which frame should be drawn in the current render loop.
 *
 */
class Animation
{
  Timer timer;
  int frameCount;
  int   loopStart{0};    // first frame to use after the first full playthrough
  bool  holdLast{false};

  public:
    Animation() : timer(0), frameCount(0) {}
    Animation(int frames, float length, int loopStartFrame = 0, bool holdLastFrame = false)
    : timer(length), frameCount(frames), loopStart(loopStartFrame), holdLast(holdLastFrame) {}

    float getLength() const { return timer.getLength(); }

    // we take a step every frame
    void step(float deltaTime) {
      timer.step(deltaTime);
      return;
    };

    /**
     * @brief currentFrame determines which frame we should draw by checking the timer, since the timer is updated every render loop, we can determine which frame should be rendered based on how far along the total animation time length we are at. Eg. if we are 0.40% done with the timer length, and we have 4 total frames, we will draw frame 0.4 * 4 = 1.6 -> cast to int truncates towards 0 -> frame 1
     */
    // int currentFrame() const {
    //   float progress = timer.getTime() / timer.getLength();      // 0..1
    //   int frame = static_cast<int>(progress * frameCount);       // 0..frameCount-1
    //   return frame;
    // };
    int currentFrame() const {
      if (frameCount == 0) return 0;

      // Optional: stay on last frame once done
      if (holdLast && timer.isTimedOut()) return frameCount - 1;

      float progress = timer.getTime() / timer.getLength(); // 0..1 (can exceed 1 when looping)
      int rawFrame = static_cast<int>(progress * frameCount);

      return rawFrame;

      // if (loopStart == 0 && rawFrame < frameCount) {
      //   return rawFrame;
      // }
      // // if (rawFrame < frameCount) return rawFrame; // first pass

      // // subsequent loops start at loopStart
      // int loopFrames = frameCount - loopStart;
      // if (loopFrames <= 0) return frameCount - 1;

      // int loopIdx = (rawFrame - frameCount) % loopFrames; // wrap within loop segment
      // return loopStart + loopIdx;
    }

    int getFrameCount() const { return frameCount; }

    bool isDone() const { return timer.isTimedOut(); }
    float getElapsed() const { return timer.getTime(); }
    void setElapsed(float elapsed, bool timedOut = false) { timer.setState(elapsed, timedOut); }

    void reset() { timer.reset(); }

};
