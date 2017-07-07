/*
 * replay_exector.h
 *
 *  Created on: Dec 14, 2016
 *      Author: yong
 */

#ifndef REPLAY_EXECTOR_H_
#define REPLAY_EXECTOR_H_

#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/thread_util.h"
#include <thread>
namespace orbit{
  class ReplayTask {
  private:
    string file_;
    string dest_folder_;
  public:
    ReplayTask() {};
    void SetFile(const string& file){
      file_ = file;
    }
    void SetDestFolder(const string& dest_folder) {
      this->dest_folder_ = dest_folder;
    }
    const string& GetFile() {
      return file_;
    }
    const string& GetDestFolder() {
      return dest_folder_;
    }
  };

  class ReplayExector {
  public:
    void AddTask(const string& file, const string& dest_folder);
  private:
    DEFINE_AS_SINGLETON(ReplayExector);

    void ReplayLoop();

    std::shared_ptr<std::thread> thread_;
    ProductQueue<std::shared_ptr<ReplayTask>> task_queue_;
    mutable std::mutex mutex_;
    bool running_ = false;
  };
}

#endif /* REPLAY_EXECTOR_H_ */
