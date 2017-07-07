/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * zk_status_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the http handler for zookeeper status page
 * ---------------------------------------------------------------------------
 */

#include "zk_status_handler.h"
#include "stream_service/orbit/http_server/html_writer.h"
#include "stream_service/orbit/http_server/build_info.h"
#include "stream_service/orbit/base/monitor/system_info.pb.h"
#include "stream_service/orbit/server/orbit_zk_client.h"
#include "stream_service/orbit/base/timeutil.h"

#include "gflags/gflags.h"

DECLARE_string(template_dir);

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


  std::string ZkStatuszHandler::RenderCpuStatus(const health::HealthStatus& status) {
    TemplateHTMLWriter temp;
    temp.LoadTemplate(kTemplate1);
    temp.SetValue("CPU_COUNT", StringPrintf("%d", status.system().cpu_number()));
    temp.SetValue("THREAD_COUNT", StringPrintf("%d", status.process().thread_count()));
    temp.SetValue("TOTAL_CPU_PERCENT", StringPrintf("%f", status.system().cpu_usage_percent()));
    temp.SetValue("MY_CPU_PERCENT", StringPrintf("%f", status.process().process_use_cpu_percent()));

    return temp.Finalize();
  }

  std::string ZkStatuszHandler::RenderMemoryStatus(const health::HealthStatus& status) {
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

  std::string ZkStatuszHandler::RenderSlaveServers() {
    vector<ServerStat> live_servers;
    orbit::zookeeper::ZookeeperRegister* zk_client = Singleton<orbit::zookeeper::ZookeeperRegister>::GetInstance();
    zk_client->Query(&live_servers);

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

      auto server_info = server.info;
      auto status = server.status;
      HTMLTableWriter::Row row;
      row.push_back(server_info.host());
      row.push_back(StringPrintf("%d", server_info.port()));
      row.push_back(server_info.name());
      row.push_back(RenderCpuStatus(status));
      row.push_back(RenderMemoryStatus(status));

      table_writer.AddRow(row);
      html_section += table_writer.Finalize();
    }
    
    return html_section;
  }

  std::shared_ptr<HttpResponse> ZkStatuszHandler::HandleHomepage(const HttpRequest& request) {
    TemplateHTMLWriter temp;
    string template_file = FLAGS_template_dir + "zkstatus.tpl";
    temp.LoadTemplateFile(template_file);
    
    temp.SetValue("SERVER_NAME", "Zookeeper.");
    temp.SetValue("GCCVERSION", __VERSION__);

    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%c",
                  std::localtime(&kBuildTimestamp));
    temp.SetValue("BUILD_USER", kBuildUser);
    temp.SetValue("BUILD_HOST", kBuildHost);
    temp.SetValue("BUILD_DATETIME", timestamp);
    temp.SetValue("BUILD_TARGET", kBazelTargetName);

    std::string service_table = RenderSlaveServers();
    temp.SetValue("SERVICE_TABLE", service_table);
    
    string html_body = temp.Finalize();
    
    std::shared_ptr<HttpResponse> http_response(new HttpResponse());
    http_response->set_code(HTTP_OK);
    http_response->set_content(html_body);
    http_response->set_content_type("text/html");
    
    return http_response;
  }

  std::shared_ptr<HttpResponse> ZkStatuszHandler::HandleRequest(const HttpRequest& request) {
    string uri = request.url();
    if (uri == "/zkstatus") {
      return HandleHomepage(request);
    }
    
    string html_body = "not found.";
    std::shared_ptr<HttpResponse> http_response(new HttpResponse());
    http_response->set_code(HTTP_NOT_FOUND);
    http_response->set_content(html_body);
    http_response->set_content_type("text/html");
    return http_response;
  }
}
