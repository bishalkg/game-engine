#pragma once

/**
 * @brief Timer
 * Timer takes in a time length.
 * It helps keep frame draws smooth by not relying strictly only on cpu cycles in the main render loop.
 * For example, when determining which frame to draw a frame sheet, we determine if the appropriate amount of time has passed before we draw the next frame by setting the next frame index, instead of drawing the next frame every loop.
*/
class Timer
{
  private:
    float length, time;
    bool timeout;

  public:
    Timer(float len) : length(len), time(0), timeout(false) {}

   /**
   * @brief takes in a deltaTime and increments the current time variable.
   * if we're past the initialized length, we set the timeout to true.
   * @param deltaTime is the time between render loops
   */
    void step(float deltaTime) {
      time += deltaTime;
      if (time >= length) {
        time -= length;
        timeout = true;
      }
    }


    bool isTimedOut() const { return timeout; }
    float getTime() const { return time; }
    float getLength() const { return length; }
    void reset() { time = 0; timeout = false; }

};