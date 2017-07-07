<!DOCTYPE html>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Statusz/</title>
<script src="//cdn.bootcss.com/jquery/2.1.4/jquery.min.js"></script>
<script src="//cdn.bootcss.com/mustache.js/2.2.1/mustache.min.js"></script>
<script type="text/javascript" src="/file?file=Chart.min.js"></script>
<link href="/file?file=statusz.css" media="all" rel="stylesheet" />
</head>
<body>
  <div>
    <h1 class="text-center titlebg">Server status from {{SERVER_NAME}}.
    </h1>
    <div class="text-left titlebg" style="border-top:1px solid;padding:3px 0px;font-size:20px">Built by {{BUILD_USER}}@{{BUILD_HOST}} on {{BUILD_DATETIME}}, using gcc-{{GCCVERSION}}, target as {{BUILD_TARGET}}
    </div>
  </div>
  <hr/>
  <div id="canvasTables">
    <h1 class="panelbg">Backend(slave) Servers</h1>
    {{SERVICE_TABLE}}
    <hr/>
    <br/>
    {{SESSION_TABLE}}
  </div>

  <script>

    window.GLOBEL_TIMMER = {};

    $(document).ready(function() {

      var tables = $('#canvasTables table');
      tables.css('margin-top','12px');

      var getDataFromIndex = function(i) {
        if(GLOBEL_TIMMER[i]) {
          clearTimeout(GLOBEL_TIMMER[i]);
          delete GLOBEL_TIMMER[i];
        }
        $.ajax({
           url: "/mstatus/get_json",
           type: "GET",
           success: function (data) {
             var json_data = eval('('+data+')');
             console.log(json_data);
             var cpuCtx = $('#cpu_'+i)[0].getContext("2d");
             DrawData(json_data[i],'CPU Percent',cpuCtx,function(obj){
               return obj.system.cpu_usage_percent;
             });
             var memeryCtx = $('#memery_'+i)[0].getContext("2d");
             DrawData(json_data[i],'Memery Useage',memeryCtx,function(obj){
               return parseInt(obj.system.used_pm_size/(1024*1024),10);
             });
             var netInCtx = $('#network_in_'+i)[0].getContext("2d");
             DrawData(json_data[i],'Network In State',netInCtx,function(obj){
               var data = obj.system.traffic.split(',');
               return getTrafficFromString(data[0]);
             });
             var netOutCtx = $('#network_out_'+i)[0].getContext("2d");
             DrawData(json_data[i],'Network Out State',netOutCtx,function(obj){
               var data = obj.system.traffic.split(',');
               return data.length >= 2 ? getTrafficFromString(data[1]) : 0;
             });
             GLOBEL_TIMMER[i] = setTimeout(function(){
                getDataFromIndex(i);
              },5000);
           },
           error: function(data) {
             alert('data load error');
           }
        });
      }

      $('.J_charts').click(function(){
        var refTr = $(this).parents('table').find('tr').last();
        var i = $(this).attr('index');
        if(refTr.css('display') == 'none') {
          refTr.show();
          
          getDataFromIndex(i);

        } else {

          if(GLOBEL_TIMMER[i]) {
            clearTimeout(GLOBEL_TIMMER[i]);
            delete GLOBEL_TIMMER[i];
          }

          refTr.hide();
        }
      });

      var getTrafficFromString = function(trafficString) {
        trafficString = trafficString.split('=');
        trafficString = trafficString.length >= 2 ? trafficString[1] : trafficString[0];
        if(trafficString.indexOf('Bytes/s') != -1) {
          return parseFloat(trafficString)/1024;
        } else if(trafficString.indexOf('KB/s') != -1) {
          return parseFloat(trafficString);
        } else if(trafficString.indexOf('MB/s') != -1) {
          return parseFloat(trafficString)*1024;
        } else {
          return 0;
        }
      }

      for(var i=0; tables[i]; i++) {
        var table = $(tables[i]);
        var tdCount = table.find('th').length;
        table.find('th').last().width(100);

        var height = 220;
        var width = table.width()/4.3;

        var cssStr = ['style="width:',width,'px;display:inline-block;border:1px solid #eee;margin-left:12px;"'].join('');

        table.find('.J_charts').attr('index',i);

        table.append(['<tr style="display:none;">',
                        '<td colspan="',tdCount,'">',
                          '<div ',cssStr,'>CPU Percent(%)<br/>',
                          '<canvas id="cpu_',i,'"></canvas>',
                          '</div>',
                          '<div ',cssStr,'>Memery Useage(MB)<br/>',
                          '<canvas id="meme cpu_usage_percent: 3.0146966461499813
  total_vm_size: 33813999616
  total_pm_size: 16730603520
  used_vm_size: 13750829056
  used_pm_size: 13264183296
  network {
  }
ry_',i,'"></canvas>',
                          '</div>',
                          '<div ',cssStr,'>Network In State(KB/s)<br/>',
                          '<canvas id="network_in_',i,'"></canvas>',
                          '</div>',
                          '<div ',cssStr,'>Network Out State(KB/s)<br/>',
                          '<canvas id="network_out_',i,'"></canvas>',
                          '</div>',
                        '</td>',
                      '</tr>'].join(''));
      }

    });

    var DrawData = function(data,label,ctx,dataCallBack) {
        var lineChartData = {
          'labels' : [],
          datasets : [
            {
              'label': label,
              fillColor : "rgba(220,220,220,0.2)",
              strokeColor : "rgba(220,220,220,1)",
              pointColor : "rgba(220,220,220,1)",
              pointStrokeColor : "#fff",
              pointHighlightFill : "#fff",
              pointHighlightStroke : "rgba(220,220,220,1)",
              data : []
            }
          ]
        };
        for(var j=0; data[j]; j++) {
          lineChartData.labels.push(j+1);
          lineChartData.datasets[0].data.push(dataCallBack(data[j]));
        }
        if(lineChartData.labels.length && ctx) {
          new Chart(ctx).Line(lineChartData, {
            animation : false,
            pointDot:false
          });
        }
    }

  </script>


</body>
</html>
