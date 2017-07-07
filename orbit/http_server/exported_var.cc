/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * varz_http_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the the exporte_var for the variables exported to /varz handler.
 * ---------------------------------------------------------------------------
 */
#include "exported_var.h"

#include "stream_service/orbit/base/strutil.h"

namespace orbit {
using namespace std;
  void ExportedVar::Set(int value) {
    Singleton<orbit::ExportedVarManager>::GetInstance()->Set(name_, value);
  }

  void ExportedVar::Increase(int diff) {
    Singleton<orbit::ExportedVarManager>::GetInstance()->Increase(name_, diff);
  }

  void ExportedVar::Decrease(int diff) {
    Singleton<orbit::ExportedVarManager>::GetInstance()->Decrease(name_, diff);
  }

  int ExportedVar::Get() {
    return Singleton<orbit::ExportedVarManager>::GetInstance()->Get(name_);
  }


  void ExportedVar::AddToManager() {
    orbit::ExportedVarManager* var_manager = Singleton<orbit::ExportedVarManager>::GetInstance();
    if (!var_manager->Has(name_)) {
      var_manager->AddVar(name_);
    }
  }

  void ExportedVar::RemoveFromManager() {
    Singleton<orbit::ExportedVarManager>::GetInstance()->RemoveVar(name_);
  }

  void ExportedVarManager::AddVar(const std::string& name) {
    std::lock_guard<std::mutex> guard(var_mutex_);
    var_map_.insert(make_pair(name, 0));
  }

  void ExportedVarManager::RemoveVar(const std::string& name) {
    std::lock_guard<std::mutex> guard(var_mutex_);
    auto it = var_map_.find(name);
    if (it != var_map_.end()) {
      var_map_.erase(it);
    }
  }

  bool ExportedVarManager::Has(const string& name) {
    std::lock_guard<std::mutex> guard(var_mutex_);
    auto search = var_map_.find(name);
    if (search != var_map_.end()) {
      return true;
    }
    return false;
  }

  int ExportedVarManager::Get(const string& name) {
    std::lock_guard<std::mutex> guard(var_mutex_);
    auto search = var_map_.find(name);
    if (search != var_map_.end()) {
      return search->second;
    }
    return -1;
  }

  string ExportedVarManager::GetVarByName(const std::string& name) {
    std::lock_guard<std::mutex> guard(var_mutex_);
    auto search = var_map_.find(name);
    if (search != var_map_.end()) {
      return GetAsString(search->second);
    }
    return "";
  }

  string ExportedVarManager::DumpExportedVars() {
    std::lock_guard<std::mutex> guard(var_mutex_);
    string text;
    ExportedVarMap::const_iterator it = var_map_.begin();
    for (; it != var_map_.end(); ++it) {
      string name = it->first;
      int value = it->second;
      StringAppendF(&text, "%s=%s\n", name.c_str(), GetAsString(value).c_str());
    }
    return text;
  }

}  // namespace orbit
