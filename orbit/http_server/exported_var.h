/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * exported_var.h
 * ---------------------------------------------------------------------------
 * Defines the exporte_var for the variables exported to /varz handler.
 * ---------------------------------------------------------------------------
 */


#ifndef EXPORTED_VAR_H__
#define EXPORTED_VAR_H__

#include "glog/logging.h"
#include <string>

#include <map>
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/strutil.h"
#include <mutex>

namespace orbit {
// Example usage (ExportedVar)
// class NiceConnection {
//  private:
//   ExportedVar connection_number_("connection_number");
// }
//  And then in the NiceConnecion::Connect() 
//   Use the following statement to increase the connection numbers variable
// void NiceConnecion::Connect() {
//   connection_number_.Increase(1);
//  ......
// }
//
// And then in the /varz handler in the browser:
//  http://HOST:PORT/varz
// and it will display
//  connection_number=100
class ExportedVar {
 public:
  ExportedVar(const std::string& name, bool should_remove = true) {
    name_ = name;
    AddToManager();
    should_remove_ = should_remove;
  }

  ExportedVar(const std::string& name, int value, bool should_remove = true) {
    name_ = name;
    AddToManager();
    Set(value);
    should_remove_ = should_remove;
  }

  ~ExportedVar() {
    if (should_remove_) {
      RemoveFromManager();
    }
  }

  std::string name() {
    return GetName();
  }

  std::string GetName() {
    return name_;
  }
  void Set(int value);

  void Increase(int diff);

  void Decrease(int diff);

  int Get();
 private:
  void AddToManager();
  void RemoveFromManager();
  std::string name_;
  bool should_remove_ = true;
};

class ExportedVarManager {
  typedef std::map<std::string, int> ExportedVarMap;
 public:
   void AddVar(const std::string& name);
   void RemoveVar(const std::string& name);
   std::string GetVarByName(const std::string& name);
   std::string DumpExportedVars();
   void Set(const std::string& name, int value) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     var_map_[name] = value;
   }
   
   void Increase(const std::string& name, int diff) {
     assert(Has(name));
     int value = Get(name);
     value += diff;
     Set(name, value);
   }
   
   void Decrease(const std::string& name, int diff) {
     assert(Has(name));
     int value = Get(name);
     value -= diff;
     if(value < 0) {
       LOG(WARNING) << "It's looks like something going wrong, name:"
                    <<  name << " descrease to " << value;
       value = 0;
     }
     Set(name, value);
   }

   bool Has(const std::string& name);
   int Get(const std::string& name);
 private:
   std::string GetAsString(int value) {
     return StringPrintf("%d", value);
   }

   std::mutex var_mutex_;
   ExportedVarMap var_map_;
   DEFINE_AS_SINGLETON(ExportedVarManager);
};
}  // namespace orbit

#endif  // EXPORTED_VAR_H__
