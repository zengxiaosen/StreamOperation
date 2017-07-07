<!DOCTYPE html>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Rpcz/ - RPC call statistics page</title>
<script src="//cdn.bootcss.com/jquery/2.1.4/jquery.min.js"></script>
<script src="//cdn.bootcss.com/mustache.js/2.2.1/mustache.min.js"></script>
<script type="text/javascript" src="/file?file=Chart.min.js"></script>
<link href="/file?file=statusz.css" media="all" rel="stylesheet" />
</head>
<body>
  <div>
    <h1 class="text-center titlebg">Rpcz - RPC call statistics page - for {{SERVER_NAME}}</h1>
  </div>
  <div>
    <h1 class="panelbg">RPC methods</h1>
    <ul>
      {{#RPC_METHOD}}
        <li>
          <div> Method: {{METHOD_NAME}} </div>
          <div>
            <table>
              <thead>
                <th>total_call</th>
                <th>pending_call</th>
                <th>success_call</th>
                <th>failed_call</th>
                <th>mean_call_time</th>
                <th>min_call_time</th>
                <th>max_call_time</th>
              </thead>
              <tbody>
                <tr>
                  <td class="text-center">{{TOTAL_CALL}}</td>
                  <td class="text-right">{{PENDING_CALL}}</td>
                  <td class="text-right">{{SUCCESS_CALL}}</td>
                  <td class="text-right">{{FAILED_CALL}}</td>
                  <td class="text-right">{{MEAN_CALL_TIME}}</td>
                  <td class="text-right">{{MIN_CALL_TIME}}</td>
                  <td class="text-right">{{MAX_CALL_TIME}}</td>
                </tr>
                <tr>
                  <td class="text-center">last 10 times call</td>
                  <td colspan="6" class="text-left">{{LAST_TEN_CALL}}</td>
                </tr>
              </tbody>
            </table>
          </div>   
          <br>
        </li>
      {{/RPC_METHOD}}
    </ul>
  </div>

  <div>
    <h1 class="panelbg">Grpc qps</h1>
    <ul>
      <li>Grpc_qps_last_20s:<b>{{GRPC_QPS_LAST_20s}}</b> </li>
    </ul>
  </div>

  <div>
    <h1 class="panelbg">Varz data</h1>
    <ul>
      <li>Stub_create:<b>{{STUB_CREATE}}</b> </li>
    </ul>
  </div>

  <div>
    <h1 class="panelbg">other interface</h1>
    <ul>
      <li>show <a href="/varz">varz</a></li>
      <li>show <a href="/statusz">statusz</a></li>
    </ul>
  </div>

</body>
</html>

