/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rpcz_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the http handler for /varz page
 * ---------------------------------------------------------------------------
 */
#include "rpcz_handler.h"
#include "rpc_call_stats.h"
#include "stream_service/orbit/base/singleton.h"
#include "exported_var.h"

#include "html_writer.h"
#include "gflags/gflags.h"

DECLARE_string(template_dir);

namespace orbit {
using namespace std;

namespace {

}  // annoymous namespace

std::shared_ptr<HttpResponse> RpczHandler::HandleRequest(const HttpRequest& request) {
  TemplateHTMLWriter temp;
  string template_file = FLAGS_template_dir + "rpcz.tpl";
  temp.LoadTemplateFile(template_file);
  temp.SetValue("SERVER_NAME", "Orbit stream server");

  RpcCallStatsManager* stats_manager = Singleton<RpcCallStatsManager>::GetInstance();
  vector<string> all_methods = stats_manager->GetAllMethods();

  for (auto method_name: all_methods) {
    TemplateHTMLWriter* section_temp;
    section_temp = temp.AddSection("RPC_METHOD");
    section_temp->SetValue("METHOD_NAME", method_name);
    RpcCallStatsData stat_data = stats_manager->GetCallStat(method_name);
    LOG(INFO) << "method_name=" << method_name;
    LOG(INFO) << "total_call=" << stat_data.total_call;
    long total_call = stat_data.total_call;
    long failed_call = stat_data.failed_call;
    long success_call = stat_data.success_call;
    long pending_call = total_call - success_call - failed_call;
    section_temp->SetValue("TOTAL_CALL", StringPrintf("%ld", total_call));
    section_temp->SetValue("SUCCESS_CALL", StringPrintf("%ld", success_call));
    section_temp->SetValue("FAILED_CALL", StringPrintf("%ld", failed_call));
    section_temp->SetValue("PENDING_CALL", StringPrintf("%ld", pending_call));

    long mean_call_time = stat_data.mean_call_time_ms;
    //long min_call_time = latencies->getPercentileEstimate(0);
    //long max_call_time = latencies->getPercentileEstimate(1);
    long min_call_time = stat_data.min_call_time_ms;
    long max_call_time = stat_data.max_call_time_ms;
    section_temp->SetValue("MEAN_CALL_TIME", StringPrintf("%ld", mean_call_time));
    section_temp->SetValue("MIN_CALL_TIME", StringPrintf("%ld", min_call_time));
    section_temp->SetValue("MAX_CALL_TIME", StringPrintf("%ld", max_call_time));
    // Update last 10 times call stat
    std::string last_ten_call_stat;
    last_ten_call_stat = stats_manager->GetLastTenTimesCallStat(method_name);
    section_temp->SetValue("LAST_TEN_CALL", last_ten_call_stat.c_str());
  }

  orbit::ExportedVarManager* var_manager = Singleton<orbit::ExportedVarManager>::GetInstance();
  temp.SetValue("STUB_CREATE", var_manager->GetVarByName("stub_create"));

  // Show the last 20 seconds grpc qps data
  temp.SetValue("GRPC_QPS_LAST_20s", stats_manager->GetGrpcQpsDataString());

  string html_body = temp.Finalize();
  std::shared_ptr<HttpResponse> http_response(new HttpResponse());
  http_response->set_code(HTTP_OK);
  http_response->set_content(html_body);
  http_response->set_content_type("text/html");
  return http_response;
}

}  // namespace orbit
