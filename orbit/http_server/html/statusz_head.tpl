<div>
    <h1 class="text-center titlebg">Statusz handler for {{SERVER_NAME}}.
    </h1>
    <div class="text-left titlebg" style="border-top:1px solid;padding:3px 0px;font-size:20px">Built by {{BUILD_USER}}@{{BUILD_HOST}} on {{BUILD_DATETIME}}, using gcc-{{GCCVERSION}}, target as {{BUILD_TARGET}}
    </div>
  </div>
  <hr/>
  <div>
    <h1 class="panelbg">other interface</h1>
    <ul>
      <li>show <a href="/varz">varz</a></li>
      <li>show <a href="/rpcz">rpcz</a></li>
      <li>show <a href="/flagz">flagz</a></li>
    </ul>
  </div>
  <div>
    <h1 class="panelbg">Display the process status</h1>
    <div style="min-height:200px;position:relative;">
      <div>
        <b>Process started:</b>{{START_TIME}}<br/>
        <b>running time:</b>{{RUNNING_TIME}} s<br/>
        <b>CPU count:</b>{{CPU_COUNT}}<br/>
        <b>Process threads count:</b>{{THREAD_COUNT}}<br/>
        <b>Total VM size:</b>{{TOTAL_VM_SIZE}}<br/>
        <b>Total VM (used) size:</b>{{TOTAL_VM_USED}}<br/>
        <b>Total VM (current process used) size:</b>{{CURRENT_PROCESS_VM_USED}}<br/>
        <b>Total PM size:</b>{{TOTAL_PM_SIZE}}<br/>
        <b>Total PM (used) size:</b>{{TOTAL_PM_USED}}<br/>
        <b>Total PM (current process used) size:</b>{{CURRENT_PROCESS_PM_USED}}<br/>
        <b>Total CPU usage percent:</b>{{TOTAL_CPU_PERCENT}}<br/>
        <b>Total CPU current process uage percent:</b>{{MY_CPU_PERCENT}}<br/>
        <b>Average CPU load:</b>{{AVG_LOAD}}<br/>
        <b>Network state:</b>{{NET_STATE}}</br>
      </div>
      <div style="width:400px;height:200px;position:absolute;right:24px;top:0;">
        <canvas id="canvas"></canvas>
      </div>
    </div>
  </div>

  <div>
    <h1 class="panelbg">Current session count:{{SESSION_COUNT_NOW}}</h1>
    <div id="sessionPanel"></div>
  </div>

  <hr/>
  <div class="text-left titlebg" style="border-top:1px solid;padding:3px 0px;font-size:20px">gRPC version:{{GRPC_VERSION}}
  </div>
