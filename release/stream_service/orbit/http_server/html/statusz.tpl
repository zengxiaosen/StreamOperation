<!DOCTYPE html>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
{{#SHOW_AUTO_REFLASH}}
<meta http-equiv="refresh" CONTENT="5" >
{{/SHOW_AUTO_REFLASH}}
<title>Statusz/</title>
<script src="//cdn.bootcss.com/jquery/2.1.4/jquery.min.js"></script>
<script src="//cdn.bootcss.com/mustache.js/2.2.1/mustache.min.js"></script>
<script type="text/javascript" src="/file?file=Chart.min.js"></script>
<link href="/file?file=statusz.css" media="all" rel="stylesheet" />
</head>
<body>
  <!-- add custom value at here -->
  <script id="sessionInfoTable" type="x-tmpl-mustache">

     <table>
	  {{#SHOW_SESSION_HEAD}}
      <thead>
        <th>session id & room id</th>
        <th>room type</th>
        <th>create time</th>
        <th>destory time</th>
        <th>stream info</th>
      </thead>
	  {{/SHOW_SESSION_HEAD}}
      <tbody>
        {{#infos}}
        <tr>
		  {{#SHOW_SESSION_INFO}}
          <td class="text-center" style="vertical-align:middle">{{session_id}}<br/>({{room_id}})</td>
          <td class="text-center" style="vertical-align:middle">{{room_type}}</td>
          <td class="text-right" style="vertical-align:middle">{{create_time}}</td>
          <td class="text-right" style="vertical-align:middle">{{destory_time}}</td>
		  {{/SHOW_SESSION_INFO}}
          <td>
            <table>
              <thead>
                <th class="text-center" style="vertical-align:middle">stream id & user name</th>
                <th class="text-center" style="vertical-align:middle">user id</th>
                <th>comming time</th>
                <th>leave time</th>
                <th>status</th>
                <th>sender bitrate</th>
                <th>receiver bitrate</th>
                <th>sender packets</th>
                <th>receiver packets</th>
                <th>sender lost</th>
                <th>receiver lost</th>
                <th>sender unit fractionlost</th>
                <th>recver unit fractionlost</th>
                <th>network status</th>
              </thead>
              <tbody>
                {{#stream_infos}}
                <tr>
                  <td class="text-center">{{stream_id}}
					{{#SHOW_BUTTON_AND_HREF}}	
					<button style="width:88px;" class="J_connectInfo">connect info</button>				
					<a href="/statusz?session_id={{session_id}}&stream_id={{stream_id}}" target="_blank">connect info</a>
					{{/SHOW_BUTTON_AND_HREF}}
					
					{{#SHOW_NOTHING}}
					<a style="width:88px;" class="J_connectInfo"></a>
					{{/SHOW_NOTHING}}
					<br/>({{user_name}})</td>
                  <td class="text-right">{{user_id}}</td>
                  <td class="text-right">{{create_time}}</td>
                  <td class="text-right">{{leave_time}}</td>
                  <td class="text-right">{{status}}</td>
                  <td class="text-right">{{custom_infos.sender_bitrate}}</td>
                  <td class="text-right">{{custom_infos.receiver_bitrate}}</td>
                  <td class="text-right">{{custom_infos.sender_packets}}</td>
                  <td class="text-right">{{custom_infos.receiver_packets}}</td>
                  <td class="text-right">{{custom_infos.sender_lost}}</td>
                  <td class="text-right">{{custom_infos.receiver_lost}}</td>
                  <td class="text-right">{{custom_infos.sender_unittime_fraction_lost}}</td>
                  <td class="text-right">{{custom_infos.receiver_unittime_fraction_lost}}</td>
                  <td class="text-right">{{custom_infos.network_status}}</td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="8" class="text-left J_charts" title="audio receive rtp packets in last 5 mins" data-array="{{custom_infos.recv_packets_audio}}" data-update="{{custom_infos.recv_packets_last_update}}">
                    <div>
                      <span style="color:red;font-weight:bold;">audio receive rtp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span>
                      <br/>
                      <canvas></canvas>
                    </div>
                  </td>
                  <td colspan="8" class="text-left J_charts" title="video receive rtp packets in last 5 mins" data-array="{{custom_infos.recv_packets_video}}" data-update="{{custom_infos.recv_packets_last_update}}">
                    <div>
                      <span style="color:red;font-weight:bold;">video receive rtp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="8" class="text-left J_charts" title="audio receive packets in last 5 mins" data-array="{{custom_infos.recv_rtcp_packets_audio}}" data-update="{{custom_infos.recv_rtcp_packets_last_update}}">
                    <div>
                      <span style="color:red;font-weight:bold;">audio receive rtcp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span>
                      <br/>
                      <canvas></canvas>
                    </div>
                  </td>
                  <td colspan="8" class="text-left J_charts" title="video receive packets in last 5 mins" data-array="{{custom_infos.recv_rtcp_packets_video}}" data-update="{{custom_infos.recv_rtcp_packets_last_update}}">
                    <div>
                      <span style="color:red;font-weight:bold;">video receive rtcp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="8" data-update="{{custom_infos.send_packets_last_update}}" class="text-left J_charts" data-array="{{custom_infos.send_packets_audio}}" title="audio send rtp packets in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">audio send rtp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.send_packets_video}}" data-update="{{custom_infos.send_packets_last_update}}" title="video send rtp packets in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">video send rtp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.send_rtcp_packets_audio}}" data-update="{{custom_infos.send_rtcp_packets_last_update}}" title="audio send rtcp packets in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">audio send rtcp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span></span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.send_rtcp_packets_video}}" data-update="{{custom_infos.send_rtcp_packets_last_update}}" title="video send rtcp packets in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">video send rtcp packets in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span></span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.packet_lost_count_recv_audio}}" data-update="{{custom_infos.packet_lost_last_update_audio}}" title="audio packets recv lost in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">audio packets recv lost in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.packet_lost_count_recv_video}}" data-update="{{custom_infos.packet_lost_last_update_video}}" title="video packets recv lost in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">video packets recv lost in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.packet_lost_count_send_audio}}" data-update="{{custom_infos.packet_lost_send_last_update_audio}}" title="audio packets send lost in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">audio packets send lost in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.packet_lost_count_send_video}}" data-update="{{custom_infos.packet_lost_send_last_update_video}}" title="video packets send lost in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">video packets send lost in last 5 mins (last update:<span class="J_lastUpdate">-</span>)</span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.rtt_data}}" data-update="{{custom_infos.rtt_data}}" title="rtt_data in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">rtt_data in last 5 mins </span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                   <td colspan="8" class="text-left J_charts" data-array="{{custom_infos.network_state}}" data-update="{{custom_infos.network_state}}" title="network_status in last 5 mins">
                    <div>
                      <span style="color:red;font-weight:bold;">network_status in last 5 mins (1:Great;2:GOOD;3:NOT_BAD;4:BAD;5:VERY_BAD;6:CANT_CONNECT) </span><br/>
                      <canvas></canvas>
                    </div>
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="2" class="text-left">changed state :</td>
                  <td colspan="14" class="text-left J_data">
                    {{custom_infos.candidateStateChanged}}
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="2" class="text-left">paired ice :</td>
                  <td colspan="14" class="text-left J_data">
                    {{custom_infos.pairedIce}}
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="1" class="text-left">local ice :</td>
                  <td colspan="7" class="text-left J_data">
                    {{custom_infos.collectedIce}}
                  </td>
                  <td colspan="1" class="text-left">remote ice :</td>
                  <td colspan="7" class="text-left J_data">
                    {{custom_infos.setRemoteIce}}
                  </td>
                </tr>
                <tr class="J_showConnectInfo" style="display:none;">
                  <td colspan="2" class="text-left">filter remote ice :</td>
                  <td colspan="14" class="text-left J_data">
                    {{custom_infos.setRemoteIceFilted}}
                  </td>
                </tr>
                {{/stream_infos}}
              </tbody>
            </table>
          </td>
        </tr>
        {{/infos}}
      </tbody>
    </table>

  </script>
  {{#SHOW_STATUSZ_HEAD}}
  {{>INCLUDE_TEMPLATE<statusz_head.tpl}}
  {{/SHOW_STATUSZ_HEAD}}
  <div id="sessionPanel"></div>
</body>
<script>
    {{>INCLUDE_TEMPLATE<js_function.tpl}}
  </script>
</html>

