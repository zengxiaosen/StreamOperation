/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * async_test.cc
 * ---------------------------------------------------------------------------
 * This program demostrates the basic usage of std::async API:
 * ---------------------------------------------------------------------------
 * Usage command line:
 * ---------------------------------------------------------------------------
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/orbit/base/timeutil.h"
#include <string>

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <numeric>
#include <future>

using namespace std;

DEFINE_string(test_op, "Read", "Test Ops: could be Create, Read or Iterate");

namespace orbit {
 
  template <typename RAIter>
  int parallel_sum(RAIter beg, RAIter end) {
    typename RAIter::difference_type len = end-beg;
    if(len < 1000000)
      return std::accumulate(beg, end, 0);
    
    RAIter mid = beg + len/2;
    auto handle = std::async(std::launch::async,
                             parallel_sum<RAIter>, mid, end);
    int sum = parallel_sum(beg, mid);
    return sum + handle.get();
  } 


  template <typename RAIter>
  int get_sum(RAIter beg, RAIter end) {
    return std::accumulate(beg, end, 0);
  }

  template <typename RAIter>
  int parallel_sum2(RAIter beg, RAIter end, int nproc) {
    typename RAIter::difference_type len = end-beg;

    std::vector<std::future<int> > res_futures;
    int nlen = len / nproc;
    for (int i = 0; i < nproc; i++) {
      RAIter mid = beg + nlen;
      if (mid > end) mid = end;
      res_futures.push_back(std::async(std::launch::async, get_sum<RAIter>, beg, mid));
      beg = mid;
    }

    int sum = 0;
    for (int i = 0; i < nproc; i++) {
      sum += res_futures[i].get();
    }

    /*
    int sum = 0;
    int done = 0;
    std::set<int> done_nproc;
    while(done_nproc.size() != nproc) {
      bool has_something_done = false;
      for (int i = 0; i < nproc; i++) {
        if (done_nproc.find(i) == done_nproc.end() && res_futures[i].valid()) {
          sum += res_futures[i].get();
          done_nproc.insert(i);
          has_something_done = true;
        }
      }
      if (!has_something_done) {
        usleep(1);
      }
    }
    */
    return sum;
  } 

  // random generator function:
  int myrandom (int i) { return std::rand()%i;}

  int RunMain() {
    std::srand ( unsigned ( std::time(0) ) );
    int size = 100000000;
    std::vector<int> v(size);
    //std::random_shuffle (v.begin(), v.end(), myrandom);
    for (int i = 0; i < size; ++i) {
      v.push_back(myrandom(100000));
    }
    for (int i = 0; i < 10; ++i) {
      long long start = getTimeMS();
      LOG(INFO) << "Sequence running sum is:" << std::accumulate(v.begin(), v.end(), 0);
      LOG(INFO) << " time:" << getTimeMS() - start << " ms";
      start = getTimeMS();
      LOG(INFO) << "Parallel running sum is:" << parallel_sum(v.begin(), v.end()) << '\n';
      LOG(INFO) << " time:" << getTimeMS() - start << " ms";
      start = getTimeMS();
      LOG(INFO) << "Parallel running sum2 is:" << parallel_sum2(v.begin(), v.end(), 10) << '\n';
      LOG(INFO) << " time:" << getTimeMS() - start << " ms";
    }
    return 0;    
  }
}  // namespace orbit


class MyClass {
public:
  MyClass() {
    LOG(INFO) << "Constructor";
  }
  ~MyClass() {
    LOG(INFO) << "Destroy....";
  }
};

class MySecondClass : public MyClass, public std::enable_shared_from_this<MyClass> {
public:
  MySecondClass() {
    LOG(INFO) << "2Constructor";
  }
  ~MySecondClass() {
    LOG(INFO) << "2Destroy....";
  }
  std::shared_ptr<MyClass> GetFromThis(){
    return shared_from_this();
  }
};

class DemoClassA;
class DemoClassB {
public:
  DemoClassB() {
    LOG(INFO) << "DemoClassB";
  }
  ~DemoClassB() {
    LOG(INFO) << "DemoClassB destroy....";
  }
  void SetValue(int value) {
    value_ = value;
  }
  void DoSomething() {
    LOG(INFO) << "dododo." << value_;
  }
  void MayTalk() {
    LOG(INFO) << "hello";
  }
  shared_ptr<DemoClassA> a;
  int value_ = 0;
};
class DemoClassA {
public:
  DemoClassA() {
    LOG(INFO) << "DemoClassA";
  }
  ~DemoClassA() {
    LOG(INFO) << "DemoClassA destroy....";
  }
  void DoB() {
    LOG(INFO) << "b.expired()=" << b.expired();
    auto k = b.lock();
    if (b.expired()) {
      LOG(INFO) << "ptr=" << k.get();
      //k->MayTalk();
    } else {
      //auto k = b.lock();
      k->DoSomething();
    }
  }
  weak_ptr<DemoClassB> b;
};


int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  //return orbit::RunMain();
  {
    //std::shared_ptr<MyClass> p1(new MyClass);
    std::shared_ptr<MySecondClass> p1;
    p1.reset(new MySecondClass);
    LOG(INFO) << p1.use_count();

    std::shared_ptr<MySecondClass> p2 = p1; //Both now own the memory.
    LOG(INFO) << p1.use_count();
    std::shared_ptr<MyClass> p3 = std::dynamic_pointer_cast<MyClass>(p1->shared_from_this());
    LOG(INFO) << p1.use_count();
    p2.reset();
    LOG(INFO) << p1.use_count();
  }

  for (int i = 0; i < 10; ++i) {
    LOG(INFO) << "i=" << i;
    std::shared_ptr<DemoClassA> da;
    da.reset(new DemoClassA);
    {
      std::shared_ptr<DemoClassB> db;
      db.reset(new DemoClassB);
      db->SetValue(100);
      da->b = std::dynamic_pointer_cast<DemoClassB>(db);
      LOG(INFO) << da.use_count();
      LOG(INFO) << db.use_count();
      db->a = da;
      LOG(INFO) << da.use_count();
      LOG(INFO) << db.use_count();
      da->DoB();
      db.reset();
      LOG(INFO) << da.use_count();
      LOG(INFO) << db.use_count();
    }
    da->DoB();
    //da->b->DoSomething();
  }

  {
    std::shared_ptr<int> sp1,sp2;
    std::weak_ptr<int> wp;
    // sharing group:
    // --------------
    sp1 = std::make_shared<int> (20);    // sp1
    LOG(INFO) << sp1.use_count();
    wp = sp1;                            // sp1, wp
    LOG(INFO) << sp1.use_count();
    
    sp2 = wp.lock();                     // sp1, wp, sp2
    LOG(INFO) << sp1.use_count();
    sp1.reset();                         //      wp, sp2
    LOG(INFO) << sp1.use_count();
    LOG(INFO) << sp2.use_count();
    
    //sp1 = wp.lock();                     // sp1, wp, sp2
    sp1 = sp2;
    LOG(INFO) << sp1.use_count();
    LOG(INFO) << sp2.use_count();
    std::cout << "*sp1: " << *sp1 << '\n';
    std::cout << "*sp2: " << *sp2 << '\n';
  }


  LOG(INFO) << "End..";
}

