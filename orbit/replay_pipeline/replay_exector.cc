/*
 * replay_exector.cc
 *
 *  Created on: Dec 14, 2016
 *      Author: yong
 */

#include "replay_exector.h"
#include "replay_pipeline.h"
namespace orbit {

void ReplayExector::AddTask(const string& file, const string& dest_folder) {
  LOG(INFO)<<"Add task";
  if(!running_) {
    LOG(INFO)<<"=============================================================Replay";
    running_ = true;
    thread_ = std::make_shared<std::thread>([this] {ReplayLoop();});
  }

  LOG(INFO)<<"===============================Set file is "<<file;
  std::shared_ptr<ReplayTask> task = std::make_shared<ReplayTask>();
  task->SetFile(file);
  task->SetDestFolder(dest_folder);
  task_queue_.Add(task);
}

void ReplayExector::ReplayLoop() {
  while(running_) {
    std::shared_ptr<ReplayTask> map_ptr;
    if (task_queue_.TryTake(&map_ptr)) {
     ReplayPipeline* pipeline = new ReplayPipeline(map_ptr->GetFile());
     pipeline->SetDestFolder(map_ptr->GetDestFolder());
     pipeline->Run();
     } else {
       /*
       * Loop task every 5s.
       */
       usleep(1000*1000*5);
   }
  }
}
}

