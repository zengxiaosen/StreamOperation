/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhihan@orangelab.cn (Zhihan He)
 *
 * session_info_main.cc
 * ---------------------------------------------------------------------------
 * Implements a test program to test the session_info.h/cc module.
 * ---------------------------------------------------------------------------
 */

#include "session_info.h"
#include "stream_service/orbit/base/singleton.h"

#include <thread>
#include <vector>
#include <memory>
#include "gflags/gflags.h"
#include "glog/logging.h"

void mainTest() {
  orbit::SessionInfoManager* session_info = Singleton<orbit::SessionInfoManager>::GetInstance();

  session_info->AddSession(123456);
  //add twice
  session_info->AddSession(123456);

  session_info->AddSession(234567);

  LOG(INFO) << session_info->GetAsJson();

  orbit::SessionInfoPtr first_session = session_info->GetSessionInfoById(123456);

  if(first_session != NULL) {
    first_session->AddStream(222);
    //stream add twice
    first_session->AddStream(222);
    first_session->AddStream(111);
    first_session->SetMoreInfo(0,"{\"room_id\":\"111111\",\"room_type\":\"AUDIO_MIXER_BRIDGE\"}");
    first_session->SetMoreInfo(222,"{\"user_name\":\"test\",\"user_id\":\"222\"}");
    //add a fake stream_id
    first_session->SetMoreInfo(222,"{\"user_name\":\"aaa\",\"user_id\":\"888\"}");
    first_session->SetStreamCustomInfo(222,"packet_lost",100);
    first_session->SetStreamCustomInfo(222,"hello","world");
    //check wether can change the value
    first_session->SetStreamCustomInfo(222,"packet_lost",200);
    first_session->SetStreamCustomInfo(222,"hello","world2");
  }

  LOG(INFO) << "SessionCountNow::" << session_info->SessionCountNow();

  //always empty
  session_info->AddSession(345678);

  orbit::SessionInfoPtr first_session2 = session_info->GetSessionInfoById(123456);
  if(first_session2 != NULL) {
    first_session2->RemoveStream(222);
    //stream remove twice
    first_session2->RemoveStream(222);
  }

  LOG(INFO) << "SessionCountNow::" << session_info->SessionCountNow();

  LOG(INFO) << session_info->GetAsJson();

  LOG(INFO) << "SessionCountNow::" << session_info->SessionCountNow();

  session_info->AddSession(456789);

  session_info->AddSession(567890);

  orbit::SessionInfoPtr second_session = session_info->GetSessionInfoById(234567);
  if(second_session != NULL) {
    //test remove a stream that not exits.
    second_session->RemoveStream(333);
    second_session->AddStream(444);
    second_session->AddStream(555);
    second_session->AddStream(666);
  }

  session_info->RemoveSession(123456);
  //session remove twice
  session_info->RemoveSession(123456);

  session_info->RemoveSession(234567);

  //the session id 123456 will at the end
  //and 234567 is before the 123456
  LOG(INFO) << session_info->GetAsJson();

  LOG(INFO) << "SessionCountNow::" << session_info->SessionCountNow();
}

void orderTest() {
  orbit::SessionInfoManager* session_info = Singleton<orbit::SessionInfoManager>::GetInstance();

  session_info->AddSession(12345671);
  usleep(1000000);
  session_info->AddSession(12345672);
  usleep(1000000);
  session_info->AddSession(12345673);
  usleep(1000000);
  session_info->AddSession(12345674);
  usleep(1000000);
  session_info->AddSession(12345675);
  usleep(1000000);
  session_info->AddSession(12345676);
  usleep(1000000);
  //should:6,5,4,2,1,3
  session_info->RemoveSession(12345673);
  LOG(INFO) << "orderTest1:" << session_info->GetAsJson();
  usleep(1000000);
  //should:6,5,4,1,2,3
  session_info->RemoveSession(12345672);
  LOG(INFO) << "orderTest2:" << session_info->GetAsJson();
  usleep(1000000);
  //should:6,5,1,4,2,3
  session_info->RemoveSession(12345674);
  LOG(INFO) << "orderTest3:" << session_info->GetAsJson();
}

void threadTestAdd() {
  int key = (rand() % 999 + 1);
  LOG(INFO) << "threadTestAdd" << "--start--" << key << "-----";
  time_t start = time(NULL);
  orbit::SessionInfoManager* session_info = Singleton<orbit::SessionInfoManager>::GetInstance();

  for(int i=0; i<10; i++) {
    session_info->AddSession(rand() % 50 + 1);
    session_info->GetAsJson();
  }

  for(int i=0; i<10; i++) {
    std::shared_ptr<orbit::SessionInfo> session = session_info->GetSessionInfoById(rand() % 100 + 1);
    if(session != NULL) {
      for(int j=0; j<20; j++) {
        int random = rand() % 100 + 1;
        session->AddStream(random);
        session_info->GetAsJson();
      }
    }
    session_info->GetAsJson();
  }

  LOG(INFO) << "session count:" << session_info->SessionCountNow();

  time_t end = time(NULL);
  LOG(INFO) << "threadTestAdd" << "--end--" << key << "----- useTime:" <<  (end-start);
}

void threadTestRemove() {
  int key = (rand() % 999 + 1);
  LOG(INFO) << "threadTestRemove" << "--start--" << key << "-----";
  time_t start = time(NULL);
  orbit::SessionInfoManager* session_info = Singleton<orbit::SessionInfoManager>::GetInstance();

  for(int i=0; i<10; i++) {
    session_info->RemoveSession(rand() % 50 + 1);
    session_info->GetAsJson();
  }

  for(int i=0; i<10; i++) {
    orbit::SessionInfoPtr session = session_info->GetSessionInfoById(rand() % 100 + 1);
    if(session != NULL) {
      for(int j=0; j<20; j++) {
        int random = rand() % 100 + 1;
        session->RemoveStream(random);
        session_info->GetAsJson();
      }
    }
    session_info->GetAsJson();
  }

  LOG(INFO) << "session count:" << session_info->SessionCountNow();

  time_t end = time(NULL);
  LOG(INFO) << "threadTestRemove" << "--end--" << key << "----- useTime:" << (end-start);
}

//if want to check max show session count,please modify MAX_SAVE_SESSION_COUNT in session_info.h
//TODO need to simulate multi thread operation
int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  mainTest();
  orderTest();

  std::vector<std::thread> threads;

  LOG(INFO) << "start";

  for(int i=0; i<100; i++) {
    threads.push_back(std::thread(threadTestAdd));
    threads.push_back(std::thread(threadTestRemove));
  }

  for (auto& thread : threads) {
    thread.join();
  }

  LOG(INFO) << "end";

  return 0;
}
