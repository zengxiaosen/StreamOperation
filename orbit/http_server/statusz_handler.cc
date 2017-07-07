/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * status_http_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the http handler for status page
 * ---------------------------------------------------------------------------
 */

#include "statusz_handler.h"
#include "html_writer.h"
#include "gflags/gflags.h"
#include "build_info.h"

#include "stream_service/orbit/base/base.h"
#include "stream_service/orbit/base/sys_info.h"
#include "stream_service/orbit/base/session_info.h"
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/strutil.h"

#include <queue>
#include <sstream>

// For gRpc version
#include <grpc/grpc.h>

DECLARE_string(template_dir);

DEFINE_string(network_interface, "eth0", "The network adapter interface to get the network stat.");

namespace orbit {
using namespace std;

  namespace {
    void ProcessRecentCpuLoads(orbit::SystemInfo* sys_info,
                               string* cpu_loads_js_array_out,
                               string* process_loads_js_array_out,
                               string* cpu_label_js_array_out) {
      queue<double> cpu_loads = sys_info->GetRecentCpuLoads();
      queue<double> process_loads = sys_info->GetRecentProcessLoads();

      string cpu_loads_js_array = "";
      string process_loads_js_array = "";
      string cpu_label_js_array = "";
      int i = 1;
      while (cpu_loads.size() > 1) {
        double load = cpu_loads.front();
        double process_load = process_loads.front();
        cpu_loads.pop();
        process_loads.pop();
        StringAppendF(&cpu_loads_js_array, "%2.2f,", load);
        StringAppendF(&cpu_label_js_array, "%d,", i);
        StringAppendF(&process_loads_js_array, "%2.2f,", process_load);
        i++;
      }
      if (cpu_loads.size() == 1) {
        double last_load = cpu_loads.front();
        cpu_loads.pop();
        double last_process_load = process_loads.front();
        process_loads.pop();
        StringAppendF(&cpu_loads_js_array, "%2.2f", last_load);
        StringAppendF(&cpu_label_js_array, "%d", i);
        StringAppendF(&process_loads_js_array, "%2.2f,", last_process_load);
      }
      CHECK(cpu_loads.empty());
      CHECK(process_loads.empty());
      *cpu_loads_js_array_out = cpu_loads_js_array;
      *cpu_label_js_array_out = cpu_label_js_array;
      *process_loads_js_array_out = process_loads_js_array;
    }

    void SetStatuszPageHead(TemplateHTMLWriter* temp) {
      temp->SetValue("SERVER_NAME", "Orbit stream server");
      temp->SetValue("GCCVERSION", __VERSION__);
      temp->SetValue("GRPC_VERSION", grpc_version_string());

      char timestamp[32];
      std::strftime(timestamp, sizeof(timestamp), "%c",
                    std::localtime(&kBuildTimestamp));
      temp->SetValue("BUILD_USER", kBuildUser);
      temp->SetValue("BUILD_HOST", kBuildHost);
      temp->SetValue("BUILD_DATETIME", timestamp);
      temp->SetValue("BUILD_TARGET", kBazelTargetName);

      orbit::SystemInfo* sys_info = Singleton<orbit::SystemInfo>::GetInstance();
      temp->SetValue("CPU_COUNT", StringPrintf("%d", sys_info->GetCpuCount()));
      temp->SetValue("THREAD_COUNT", StringPrintf("%d", sys_info->GetCurrentProcessThreadCount()));
      temp->SetValue("TOTAL_VM_SIZE", StringPrintf("%d MB", sys_info->GetTotalVM() / (1024 * 1024) ));
      temp->SetValue("TOTAL_VM_USED", StringPrintf("%d MB", sys_info->GetTotalUsedVM() / (1024 * 1024) ));
      temp->SetValue("CURRENT_PROCESS_VM_USED", StringPrintf("%d MB", sys_info->GetUsedVMByCurrentProcess() / 1024));

      temp->SetValue("TOTAL_PM_SIZE", StringPrintf("%d MB ", sys_info->GetTotalPhysicalMemory() / (1024 * 1024) ));
      temp->SetValue("TOTAL_PM_USED", StringPrintf("%d MB", sys_info->GetTotalUsedPhysicalMemory() / (1024 * 1024) ));
      temp->SetValue("CURRENT_PROCESS_PM_USED", StringPrintf("%d MB", sys_info->GetUsedPhysicalMemoryByCurrentProcess() / 1024));

      temp->SetValue("TOTAL_CPU_PERCENT", StringPrintf("%f", sys_info->GetTotalCpuLoad()));
      temp->SetValue("MY_CPU_PERCENT", StringPrintf("%f", sys_info->GetCurrentProcessCpuLoad()));

      stats_load st_load = sys_info->GetAvgLoad();
      temp->SetValue("AVG_LOAD",
                    StringPrintf("load_avg_1=%u,load_avg_5=%u,load_avg_15=%u",
                                 st_load.load_avg_1,
                                 st_load.load_avg_5,
                                 st_load.load_avg_15));
      temp->SetValue("NET_STATE", sys_info->GetReadableTrafficStat(FLAGS_network_interface));

      string cpu_loads_js_array;
      string process_loads_js_array;
      string cpu_label_js_array;
      ProcessRecentCpuLoads(sys_info,
                            &cpu_loads_js_array,
                            &process_loads_js_array,
                            &cpu_label_js_array);

      temp->SetValue("CPU_LOADS", cpu_loads_js_array);
      temp->SetValue("PROCESS_LOADS", process_loads_js_array);
      temp->SetValue("CPU_LABELS", cpu_label_js_array);

      temp->SetValue("START_TIME", sys_info->GetStartTime());
      temp->SetValue("RUNNING_TIME", StringPrintf("%d", sys_info->GetElapsedTime()));

      orbit::SessionInfoManager* session_info = 
        Singleton<orbit::SessionInfoManager>::GetInstance();
      temp->SetValue("SESSION_COUNT_TOTAL",StringPrintf("%d",session_info->SessionCountTotal()));
      temp->SetValue("SESSION_COUNT_NOW",StringPrintf("%d",session_info->SessionCountNow()));
    }

    std::shared_ptr<HttpResponse> ResponseStatuszPage(std::string room_id = "") {
      TemplateHTMLWriter temp;
      string template_file = "statusz.tpl";
      temp.LoadTemplateFile(FLAGS_template_dir + template_file);

      SetStatuszPageHead(&temp);

      temp.AddSection("SHOW_SESSION_HEAD");
      temp.AddSection("SHOW_SESSION_INFO");
      temp.AddSection("SHOW_BUTTON_AND_HREF");
      temp.AddSection("SHOW_CLICK");
      temp.AddSection("SHOW_CLICK1");
      temp.AddSection("SHOW_STATUSZ_HEAD");
      temp.AddSection("show_statusz_head");
      
      orbit::SessionInfoManager* session_info = 
        Singleton<orbit::SessionInfoManager>::GetInstance();
      temp.SetValue("SESSION_INFO_JSON",session_info->GetAsJson(room_id));

      string html_body = temp.Finalize();

      std::shared_ptr<HttpResponse> http_response(new HttpResponse());
      http_response->set_code(HTTP_OK);
      http_response->set_content(html_body);
      http_response->set_content_type("text/html");
      return http_response;
    }

    std::shared_ptr<HttpResponse> ResponseUserStatuszPage(int session_id, int stream_id) {
      TemplateHTMLWriter temp;
      string template_file = FLAGS_template_dir + "statusz.tpl";
      temp.LoadTemplateFile(template_file);

      temp.AddSection("SHOW_NOTHING");
      temp.AddSection("SHOW_DIRECT");
      temp.AddSection("SHOW_AUTO_REFLASH");
      
      SetStatuszPageHead(&temp);
      
      orbit::SessionInfoManager* session_info =
        Singleton<orbit::SessionInfoManager>::GetInstance();
      temp.SetValue("SESSION_INFO_JSON", session_info->GetAsJson(session_id, stream_id));

      string html_body = temp.Finalize();
      
      std::shared_ptr<HttpResponse> http_response(new HttpResponse());
      http_response->set_code(HTTP_OK);
      http_response->set_content(html_body);
      http_response->set_content_type("text/html");

      return http_response;
    }
  }  // annoymouse namespace

StatuszHandler::StatuszHandler() {
}

std::shared_ptr<HttpResponse> StatuszHandler::HandleRequest(const HttpRequest& request) {
  bool b_search = false;
  std::shared_ptr<HttpResponse> http_response;

  int int_session_id = 0;
  int int_stream_id = 0;
  std::string room_id = "";

  if (request.GetQueryList().size() > 0) {
    std::string session_id = "";
    std::string stream_id = "";
    b_search = (request.GetQueryValue("session_id", &session_id) &
      request.GetQueryValue("stream_id", &stream_id)) |
      request.GetQueryValue("room_id", &room_id);

    std::stringstream ss;
    if (session_id.length() > 0) {
      ss << session_id;
      ss >> int_session_id;
    }

    if (stream_id.length() > 0) {
      ss.clear();
      ss << stream_id;
      ss >> int_stream_id;
    }
    
    LOG(INFO) << "session_id : " << int_session_id
              << " stream_id : " << int_stream_id
              << " room_id : " << room_id
              << " b_search : " << b_search;
  }

  if (b_search) {
    if (int_session_id != 0 && int_stream_id != 0) {
      http_response = ResponseUserStatuszPage(int_session_id, int_stream_id);
    } else if (room_id.length() > 0) {
      http_response = ResponseStatuszPage(room_id);
    } else {
      LOG(INFO) << "Request parameter is error !";
    }
  } else {
    http_response = ResponseStatuszPage();
  }

  return http_response;
}

}
