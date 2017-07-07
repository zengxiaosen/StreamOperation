/*
 * video_map_test.cc
 *
 *  Created on: 2016-9-18
 *      Author: cxy
 */

#include <iostream>
#include <string>
#include <vector>

#include "video_map.h"

#include "glog/logging.h"
#include "gflags/gflags.h"

using namespace std;

void PrintVideoMap(const orbit::VideoMap *map) {
  cout << "master : " << map->GetMaster() << endl;
  vector<orbit::VideoMap::id_t> vec;
  map->GetAllId(&vec);
  for (auto id : vec) {
    printf("%3ld see %3ld , expect %3ld\n", id, map->GetSrc(id), map->GetExpectedSrc(id));
  }
  cout << "======================================" << endl;
}

int main(int argc, char *argv[]) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  orbit::VideoMap *map = new orbit::VideoMap;
  map->SetPrepareCallback([](orbit::VideoMap::id_t id) {
    printf("prepare: %3ld\n", id);
  });
  map->AddDisabledListener([](orbit::VideoMap::id_t id) {
    printf("disable: %3ld\n", id);
  });

  while (cin) {
    string cmd;
    int id;
    cin >> cmd;
    if (cmd == "q") {
      break;
    }
    if (cmd == "add") {
      cin >> id;
      map->Add(id);
    } else if (cmd == "prep") {
      cin >> id;
      map->SetPrepared(id);
    } else if (cmd == "rm") {
      cin >> id;
      map->Remove(id);
    } else if (cmd == "set") {
      cin >> id;
      map->SetMaster(id);
    }

    PrintVideoMap(map);

  }

  delete map;
  return 0;
}


