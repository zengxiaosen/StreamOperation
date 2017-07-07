/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * master_status_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the http handler for /mstatus page
 * ---------------------------------------------------------------------------
 */
#include "master_status_handler.h"
#include "stream_service/orbit/http_server/html_writer.h"
#include "stream_service/orbit/http_server/build_info.h"
#include "stream_service/orbit/base/monitor/system_info.pb.h"
#include "orbit_master_server_manager.h"
#include "gflags/gflags.h"

namespace orbit {
using namespace std;
const string kTemplate1 = "" \
"      <div> " \
"        <b>CPU count:</b>{{CPU_COUNT}}<br/> " \
"        <b>Process threads count:</b>{{THREAD_COUNT}}<br/>" \
"        <b>Total CPU usage percent:</b>{{TOTAL_CPU_PERCENT}}<br/> " \
"        <b>Total CPU current process uage percent:</b>{{MY_CPU_PERCENT}}<br/> " \
"      </div>";

const string kTemplate2 = "" \
"      <div> " \
"        <b>Total VM size</b>{{TOTAL_VM_SIZE}}<br/> " \
"        <b>Total VM (used) size:</b>{{TOTAL_VM_USED}}<br/>" \
"        <b>Total VM (current process used) size:</b>{{CURRENT_PROCESS_VM_USED}}<br/>" \
"        <b>Total PM size:</b>{{TOTAL_PM_SIZE}}<br/>" \
"        <b>Total PM (used) size:</b>{{TOTAL_PM_USED}}<br/> " \
"        <b>Total PM (current process used) size:</b>{{CURRENT_PROCESS_PM_USED}}<br/> " \
"      </div>";

const string kTemplate3 = "" \
"      <div> " \
"        <b>Average CPU load:</b>{{AVG_LOAD}}<br/> " \
"        <b>Network state:</b>{{NET_STATE}}</br> " \
"      </div>";


std::string MasterStatusHandler::RenderCpuStatus(const health::HealthStatus& status) {
    TemplateHTMLWriter temp;
    temp.LoadTemplate(kTemplate1);
    temp.SetValue("CPU_COUNT", StringPrintf("%d", status.system().cpu_number()));
    temp.SetValue("THREAD_COUNT", StringPrintf("%d", status.process().thread_count()));
    temp.SetValue("TOTAL_CPU_PERCENT", StringPrintf("%f", status.system().cpu_usage_percent()));
    temp.SetValue("MY_CPU_PERCENT", StringPrintf("%f", status.process().process_use_cpu_percent()));

    return temp.Finalize();
  }

  std::string MasterStatusHandler::RenderMemoryStatus(const health::HealthStatus& status) {
    TemplateHTMLWriter temp;
    temp.LoadTemplate(kTemplate2);
    temp.SetValue("TOTAL_VM_SIZE", StringPrintf("%d MB", status.system().total_vm_size() / (1024 * 1024) ));
    temp.SetValue("TOTAL_VM_USED", StringPrintf("%d MB", status.system().used_vm_size() / (1024 * 1024) ));
    temp.SetValue("CURRENT_PROCESS_VM_USED", StringPrintf("%d MB", status.process().used_vm_process() / 1024));
    
    temp.SetValue("TOTAL_PM_SIZE", StringPrintf("%d MB ", status.system().total_pm_size() / (1024 * 1024) ));
    temp.SetValue("TOTAL_PM_USED", StringPrintf("%d MB", status.system().used_pm_size() / (1024 * 1024) ));
    temp.SetValue("CURRENT_PROCESS_PM_USED", StringPrintf("%d MB", status.process().used_pm_process() / 1024));
    return temp.Finalize();
  }

  std::string MasterStatusHandler::RenderLoadStatus(const health::HealthStatus& status) {
    TemplateHTMLWriter temp;
    temp.LoadTemplate(kTemplate3);
    temp.SetValue("AVG_LOAD",
                  StringPrintf("load_avg_1=%u<br/>load_avg_5=%u<br/>load_avg_15=%u",
                               status.system().load().load_avg_1(),
                               status.system().load().load_avg_5(),
                               status.system().load().load_avg_15()));
    temp.SetValue("NET_STATE", status.system().network().nic_readable_traffic());
    return temp.Finalize();
  }

  std::string MasterStatusHandler::RenderSlaveServers(OrbitMasterServerManager* manager) {
    vector<ServerStat> live_servers = manager->GetRawLiveServers();

    string html_section;

    for (auto server : live_servers) {
      HTMLTableWriter table_writer;
      table_writer.set_has_border();
      table_writer.set_border("1px");

      table_writer.AddColumn("HOST");
      table_writer.AddColumn("PORT");
      table_writer.AddColumn("SERVICE_NAME");
      table_writer.AddColumn("CPU");
      table_writer.AddColumn("MEMORY");
      table_writer.AddColumn("LOAD");
      table_writer.AddColumn("OPERATION");

      auto server_info = server.info;
      auto status = server.status;
      HTMLTableWriter::Row row;
      row.push_back(server_info.host());
      row.push_back(StringPrintf("%d", server_info.port()));
      row.push_back(server_info.name());
      row.push_back(RenderCpuStatus(status));
      row.push_back(RenderMemoryStatus(status));
      row.push_back(RenderLoadStatus(status));
      /*
      row.push_back(StringPrintf("<canvas uuid=\"%s_%d\"></canvas>", 
                                  server_info.host().c_str() ,
                                  server_info.port()));
       */
      row.push_back(StringPrintf("<a target='_blank' href='https://%s:%d/statusz'>Link</a>&nbsp;&nbsp;&nbsp;&nbsp;<button class='J_charts'>status</button>",
                                 server_info.host().c_str(),
                                 server_info.http_port()));
      table_writer.AddRow(row);
      html_section += table_writer.Finalize();
    }
    return html_section;
    //return table_writer.Finalize();
  }

  std::string MasterStatusHandler::RenderSessionServerInfo(OrbitMasterServerManager* manager) {
    map<int, registry::ServerInfo> session_to_server = manager->GetSessionServerMap();
    map<string, vector<int> > server_with_sessions;
    for (auto item : session_to_server) {
      registry::ServerInfo server_info = item.second;
      string key = StringPrintf("%s:%d", server_info.host().c_str(), server_info.port());
      //server_with_sessions.insert(make_pair(key, item.first));
      vector<int> sessions = server_with_sessions[key];
      sessions.push_back(item.first);
      server_with_sessions[key] = sessions;
    }
    HTMLTableWriter table_writer;
    table_writer.set_has_border();
    table_writer.set_border("1px");

    table_writer.AddColumn("SERVER");
    table_writer.AddColumn("SESSION");

    for (map<string, vector<int> >::iterator item = server_with_sessions.begin();
         item != server_with_sessions.end(); item++) {
      string server_info = item->first;
      vector<int> sessions = item->second;
      HTMLTableWriter::Row row;
      row.push_back(server_info);
      string session_str;
      for (auto session : sessions) {
        session_str += StringPrintf("%d,", session);
      }
      row.push_back(session_str);
      table_writer.AddRow(row);
    }

    return table_writer.Finalize();
  }

  std::shared_ptr<HttpResponse> MasterStatusHandler::HandleGetJsonData(const HttpRequest& request) {
    OrbitMasterServerManager* manager = Singleton<OrbitMasterServerManager>::GetInstance();
    SlaveDataCollector* collector = Singleton<SlaveDataCollector>::GetInstance();
    vector<ServerStat> live_servers = manager->GetRawLiveServers();

    //temp.SetValue("SERVICE_DATAS",return_json);

    string return_json = "[";
    for (auto server : live_servers) {
      auto server_info = server.info;
      string data_prefix = StringPrintf("%s:%d",server_info.host().c_str(),
                                        server_info.port());
      long current_second = getTimeMS()/1000;
      string limit = StringPrintf("%s_%d",data_prefix.c_str(),current_second);
      string start = StringPrintf("%s_%d",data_prefix.c_str(),current_second-30 * 10);
      auto datas = collector->Find(start,limit);
      return_json += "[";
      for(auto data : datas) {
        string data_str =
          HealthStatusToJsonData(server_info.host(), server_info.port(), data);
        return_json += data_str + ",";
      }
      if(',' == return_json.back()) {
        return_json.pop_back();
      }
      return_json += "],";
    }
    if(',' == return_json.back()) {
      return_json.pop_back();
    }
    return_json += "]";

    std::shared_ptr<HttpResponse> http_response(new HttpResponse());
    http_response->set_code(HTTP_OK);
    http_response->set_content(return_json);
    //http_response->set_content_type("application/json");
    http_response->set_content_type("text/html");
    return http_response;
  }

  std::shared_ptr<HttpResponse> MasterStatusHandler::HandleHomepage(const HttpRequest& request) {
    TemplateHTMLWriter temp;
    string template_file = root_dir_ + "mstatus.tpl";
    temp.LoadTemplateFile(template_file);
    
    temp.SetValue("SERVER_NAME", "Orbit Master (stream server).");
    temp.SetValue("GCCVERSION", __VERSION__);

    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%c",
                  std::localtime(&kBuildTimestamp));
    temp.SetValue("BUILD_USER", kBuildUser);
    temp.SetValue("BUILD_HOST", kBuildHost);
    temp.SetValue("BUILD_DATETIME", timestamp);
    temp.SetValue("BUILD_TARGET", kBazelTargetName);
    
    OrbitMasterServerManager* manager = Singleton<OrbitMasterServerManager>::GetInstance();
    std::string service_table = RenderSlaveServers(manager);
    temp.SetValue("SERVICE_TABLE", service_table);
    std::string session_html = RenderSessionServerInfo(manager);
    temp.SetValue("SESSION_TABLE", session_html);

    string html_body = temp.Finalize();
    
    std::shared_ptr<HttpResponse> http_response(new HttpResponse());
    http_response->set_code(HTTP_OK);
    http_response->set_content(html_body);
    http_response->set_content_type("text/html");
    
    return http_response;
  }

  std::shared_ptr<HttpResponse> MasterStatusHandler::HandleRequest(const HttpRequest& request) {
    string uri = request.url();
    if (uri == "/mstatus") {
      return HandleHomepage(request);
    } else if (uri == "/mstatus/get_json") {
      return HandleGetJsonData(request);
    }
    string html_body = "not found.";
    std::shared_ptr<HttpResponse> http_response(new HttpResponse());
    http_response->set_code(HTTP_NOT_FOUND);
    http_response->set_content(html_body);
    http_response->set_content_type("text/html");
    return http_response;
  }


}  // namespace orbit
