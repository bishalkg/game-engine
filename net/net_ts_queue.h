#pragma once
#include "net_common.h"

namespace net
{

  template<typename T>
  class tsqueue
  {
    public:
      tsqueue() = default;
      tsqueue(const tsqueue<T>&) = delete; // dont allow queue to be copied
      virtual ~tsqueue() { clear(); } // cleanup when tsqueue is destroyed


      public:
        const T& front()
        {
          std::scoped_lock lock(mu); // pass mutex to scoped lock constructor; mutex is held until destructor is called at the end of the scope of the fn
          return deQueue.front();
        }

        const T& back()
        {
          std::scoped_lock lock(mu);
          return deQueue.back();
        }

        void push_back(const T& item)
        {
          std::scoped_lock lock(mu);
          deQueue.emplace_back(std::move(item));

          std::unique_lock<std::mutex> ul(muxBlocking);
          cvBlocking.notify_one();
        }

        void push_front(const T& item) {
          std::scoped_lock lock(mu);
          deQueue.emplace_front(std::move(item));

          std::unique_lock<std::mutex> ul(muxBlocking);
          cvBlocking.notify_one();
        }

        bool empty() {
          std::scoped_lock lock(mu);
          return deQueue.empty();
        }

        size_t count() {
          std::scoped_lock lock(mu);
          return deQueue.size();
        }

        void clear() {
          std::scoped_lock lock(mu);
          return deQueue.clear();
        }

        T pop_front() {
          std::scoped_lock lock(mu);
          auto item = std::move(deQueue.front());
          deQueue.pop_front();
          return item;
        }

        T pop_back() {
          std::scoped_lock lock(mu);
          auto item = std::move(deQueue.back());
          deQueue.pop_back();
          return item;
        }

        // wait for condition variable to notify there is data in queue
        void wait() {
          while (empty() && !stop.load()){
            // wait needs unique_lock because it must unlock/relock around the sleep; scoped_lock can’t do that.
            std::unique_lock<std::mutex> ul(muxBlocking);
            cvBlocking.wait(ul);
          }
        }

        void wakeAll() {
          stop.store(true);
          cvBlocking.notify_all();
        }

    protected:
      std::mutex mu;
      std::deque<T> deQueue;

      std::condition_variable cvBlocking;
      std::mutex muxBlocking;
      std::atomic<bool> stop{false};

  };

}