var randomScalingFactor = function(){ return Math.round(Math.random()*100)};
    var lineChartData = {
      labels : [{{CPU_LABELS}}],
      datasets : [
        {
          label: "CPU Percent",
          fillColor : "rgba(220,220,220,0.2)",
          strokeColor : "rgba(220,220,220,1)",
          pointColor : "rgba(220,220,220,1)",
          pointStrokeColor : "#fff",
          pointHighlightFill : "#fff",
          pointHighlightStroke : "rgba(220,220,220,1)",
          data : [{{CPU_LOADS}}]
        },
        {
            label: "Process CPU Percent",
            fillColor: "rgba(151,187,205,0.2)",
            strokeColor: "rgba(151,187,205,1)",
            pointColor: "rgba(151,187,205,1)",
            pointStrokeColor: "#fff",
            pointHighlightFill: "#fff",
            pointHighlightStroke: "rgba(151,187,205,1)",
            data : [{{PROCESS_LOADS}}]
        }
      ]
    }
  window.onload = function(){
    var ctx = document.getElementById("canvas").getContext("2d");
    window.myLine = new Chart(ctx).Line(lineChartData, {
      responsive: true
    });
  }

  $(document).ready(function(){
    var sessionInfos = {'infos':{{SESSION_INFO_JSON}}};

    function loadSessionInfo(infos) {
      var template = $('#sessionInfoTable').html();
      Mustache.parse(template);
      var rendered = Mustache.render(template, infos);
      $('#sessionPanel').html(rendered);
    }

    if(sessionInfos.infos.length <= 0) {
      $('#sessionPanel').html('empty now');
    } else {
      loadSessionInfo(sessionInfos);
    }

	{{#SHOW_CLICK}}
    $(document.body).on('click','.J_connectInfo',function(){
      var infoContent = $(this).parents('tr').first();
	{{/SHOW_CLICK}}

	  {{#SHOW_DIRECT}}
	  var infoContent = $('.J_connectInfo').parents('tr').first();
	  {{/SHOW_DIRECT}}
      var next = infoContent.next();
      while(next[0] && next.hasClass('J_showConnectInfo')) {
        if (next.css('display').toUpperCase() == 'NONE') {
          var data = next.find('.J_data');
          for(var i=0; data[i]; i++) {
            var refElem = $(data[i]);
            var htmls = refElem.text().split(',');
            if(htmls.length > 1) {
              refElem.html(htmls.join('<br/><br/>'))
            }
          }
          next.show();
          var chartData = next.find('.J_charts');
          for(var i=0; chartData[i]; i++) {
            try {
              var refElm = $(chartData[i]);
              var data = refElm.data('array');
              data = eval('['+data+']');
              var canvas = refElm.find('canvas');
              canvas.css({'width':'600px','height':'300px'});
              var lastUpdate = refElm.data('update');
              if(lastUpdate) {
                var date = new Date(lastUpdate);
                date = ('' + date).toString().split(' ');
                date = date[date.length-3];
                refElm.find('.J_lastUpdate').text(date);
              }
              var ctx = canvas[0].getContext('2d');
              //好像和0有关，不对，另外video的send是0，记录数据的方式也不对，会丢一些数据
              DrawData(data,refElm.title,ctx,function(obj){
                return obj;
              });
            } catch(e){
              console.log('got run chart data:',e);
            }
          }
        } else {
          next.hide();
        }
        next = next.next();
      }
	{{#SHOW_CLICK1}}
    });
	{{/SHOW_CLICK1}}
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
      for(var j=0; j<data.length; j++) {
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
