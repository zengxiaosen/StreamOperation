/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * singleton.h
 * ---------------------------------------------------------------------------
 * Defines the singleton template class for easily creating and integrating
 * the singleton classes.
 * ---------------------------------------------------------------------------
 * 
 */

#ifndef __SINGLETON_
#define __SINGLETON_
#include <boost/thread.hpp>

// Example usage of Singleton)
// --------------------------------------------------------------------------
//  To define a class as a singleton class, use the DEFINE_AS_SINGLETON()
//  macro in the private section of the class.
//
// For example:
// class NiceConnection {
//  public:
//    void Send(DataPacket packet);
//  private:
//    DEFINE_AS_SINGLETON(NiceConnection);
// }
//
// To use the singleton class, use the Singleton<> template class with the
//  GetInstance() method to instantiate the class as an instance:
//    NiceConnection* nice = Singleton<NiceConnection>::GetInstance();
//    nice->Send(packet);
//
template <class T>
class Singleton {
 public:
  static T* get() {
    return GetInstance();
  }

  T& operator*() {
    return *get();
  }

  T* operator->() {
    return get();
  }

  static T* GetInstance() {
    static Cleanup _Cleanup;
    {
      boost::mutex::scoped_lock lock(*instance_mutex_);
      if (!instance_) {
        instance_ = new T();
      }
    }
    return instance_;
  }
protected:
  friend class Cleanup;
  class Cleanup {
    public:
    ~Cleanup() {
      boost::mutex::scoped_lock lock(*instance_mutex_);
      if (instance_) {
        delete instance_;
        instance_ = NULL;
      }
    }
  };
private:
  Singleton();
  ~Singleton();

  static boost::mutex* instance_mutex_;
  static T* instance_;
};

template <class T> T* Singleton<T>::instance_ = NULL;
template <class T> boost::mutex* Singleton<T>::instance_mutex_ = new boost::mutex;
#define DEFINE_AS_SINGLETON(T) friend class Singleton<T>; \
                               T(){}; \
                               ~T(){}
#define DEFINE_AS_SINGLETON_WITHOUT_CONSTRUCTOR(T) friend class Singleton<T>; \
  T(); \
  ~T()

#endif  // SINGLETON__
