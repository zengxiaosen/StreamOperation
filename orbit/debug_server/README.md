README for debug server
-------------------------

Orbit Debug Server is used to debug the orbit media server using the orbit client itself. More specificially, it will use the orbit to act as a client to connect to the orbit server or other WebRTC server, and then interacts with them using the real captured RTP packets to simulate the real environment.

Currently, the debug server has several components:
* RTP capture - use to capture the real RTP packets and then store them into a recordio file.
* RTP replay - use to replay the stored RTP packets into the real system.

Guideline:

1. How to capture the screen session

Right now the orbit stream server has a video element(component) named stream_recorder_element. The element is used to connect with the plugin (i.e. audio_conference and video_mixer etc.) and then the RTP packets will be passed into the component and then the GStreamer pipeline (to decode the RTP packet and then store into a file) in the component will be started and running, to record the session into a webm file.

The flags is used to enable the functions to capture the RTP packets. The flags are:
--video_mixer_use_record_stream     and
--audio_conference_use_record_stream

bazel-bin/stream_service/orbit/server/orbit_stream_server --logtostderr --use_plugin_name="video_mixer" --video_mixer_use_record_stream=true --record_path=stream_service/orbit/http_server/html/xucheng6.webm


2. How to capture the RTP packets

A flag is used to enable the function to capture RTP packet. The flag is
--packet_capture_filename

It will specify the captured file's name. The RTP packets will be stored into the file using a recordio format. 

bazel-bin/stream_service/orbit/server/orbit_stream_server --logtostderr --packet_capture_filename=record9.pb


3. How to replay the RTP packets.

rtp_replay_main is the main program to replay the RTP packets and simulate the pipeline and plugin that has been specified by the flag.
The flags are used:
--replay_files  to specify the files to be replayed. You can specify multiple replay files. Just use ";" as the delimiter to separate each file.

--test_plugin   to specify the types which plugin and room are used. Could be video_mixer or audio_conference

Example usage:

bazel-bin/stream_service/orbit/debug_server/rtp_replay_main --logtostderr --replay_files="./record8.pb;./record9.pb" --v=2 --test_plugin="video_mixer"

4. Use the GStreamer pipeline to render/replay the video

By default, the pipeline we are running above is to replay the rtp packets and attach to a stream_recoder_element to record the video.
The video's default path and location is /tmp/orbit_recorder/default.webm
Video codecs used in this webm file is VP8
Thus, we can use the Gstreamer pipeline like this to replay it:
(In MacOSX machine, we probaly need to add the gst-launch-1.0 to the PATH: by set the export PATH="$PATH:/Library/Frameworks/GStreamer.framework/Commands/")

gst-launch-1.0 filesrc location=~/project/repository/trunk/default.webm ! matroskademux ! vp8dec ! autovideosink
