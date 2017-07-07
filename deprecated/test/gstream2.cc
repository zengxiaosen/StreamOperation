// g++ -fpermissive -o gstream2 gstream2.cc `pkg-config --cflags --libs gstreamer-1.5`
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

GstElement* GetDemoPipeline() {
  printf ("Start Parse: \n");
  GError* error = NULL;

  GstElement* pipeline = gst_parse_launch("filesrc location=/home/chengxu/xucheng.mp4 ! decodebin !  autovideosink", &error);

  printf ("End Parse: \n");

  return pipeline;
}

 void cb_newpad (GstElement* decodebin,GstPad* pad,gpointer user_data) 
{ 
    printf("add new pad\n"); 
    GstPad *audiopad; 
    GstPad *videopad; 

    GstElement* videosink = (GstElement*) user_data;

    videopad = gst_element_get_static_pad (videosink, "sink"); 
    if (GST_PAD_IS_LINKED (videopad)) { 
      printf("video has linked\n"); 
      g_object_unref (videopad); 
      return; 
    } 

    if(gst_pad_link(pad,videopad) !=0) 
      { 
        printf("link video error\n"); 
      } 
    gst_object_unref (videopad); 
} 

GstElement* ConstructPipeline() {
  /* Create the empty pipeline */
  GError* error = NULL;

  GstElement* pipeline = gst_pipeline_new ("test-pipeline");

  GstElement* video_src = gst_parse_launch("filesrc location=/home/chengxu/xucheng.mp4", &error);
  GstElement* decode_bin = gst_element_factory_make ("decodebin", NULL);
  GstElement* video_sink = gst_element_factory_make ("autovideosink", NULL);
  g_signal_connect (decode_bin, "pad-added", G_CALLBACK (cb_newpad), video_sink); 

  gst_bin_add_many (GST_BIN (pipeline), video_src, decode_bin, video_sink, NULL);

  //gst_element_link_many (video_src, decode_bin, video_sink, NULL);
  if (!gst_element_link(video_src, decode_bin)) {
    cout << "link video_src to decode_bin fails." << endl;
  }

  if (!gst_element_link(decode_bin, video_sink)) {
    cout << "link decode_bin to videosink fails." << endl;
  }

  /*
  if (!gst_element_link_many (video_src, decode_bin, video_sink, NULL)) {
    //LOG(FATAL) << "Link video_src and video_enc fails.";
    printf ("Link elements error: \n");
    exit (1);
  }
*/
  return pipeline;
}

GstElement* ConstructComplexPipeline() {
  /* Create the empty pipeline */
  GError* error = NULL;

  GstElement* pipeline = gst_pipeline_new ("test-pipeline");

  GstElement* video_src = gst_parse_launch("filesrc location=/home/chengxu/xucheng.mp4", &error);
  GstElement* decode_bin = gst_element_factory_make ("decodebin", NULL);
  GstElement* video_sink = gst_element_factory_make ("autovideosink", NULL);
  g_signal_connect (decode_bin, "pad-added", G_CALLBACK (cb_newpad), video_sink); 

  gst_bin_add_many (GST_BIN (pipeline), video_src, decode_bin, video_sink, NULL);

  //gst_element_link_many (video_src, decode_bin, video_sink, NULL);
  if (!gst_element_link(video_src, decode_bin)) {
    cout << "link video_src to decode_bin fails." << endl;
  }

  if (!gst_element_link(decode_bin, video_sink)) {
    cout << "link decode_bin to videosink fails." << endl;
  }

  return pipeline;
}


int main(int argc, char *argv[]) {   
  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  //GstElement *pipeline;

  /* Create the empty pipeline */
  //pipeline = gst_pipeline_new ("test-pipeline");

  GError* error = NULL;
  GstMessage *msg;
  GstBus *bus;

  // filesrc location=/home/chengxu/xucheng.mp4 ! decodebin !  autovideosink 
  // queue ! videoconvert ! videoscale ! videorate !  video/x-raw,width=100,height=120,framerate=15/1 !

  //GstElement* pipeline = GetDemoPipeline();
  GstElement* pipeline = ConstructPipeline();
  if (!pipeline) {
    //printf ("Parse error: %s\n", error->message);
    printf ("Parse error: \n");
    exit (1);
  }
  
  printf ("Set status.: \n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus (pipeline);

  /* wait until we either get an EOS or an ERROR message. Note that in a real
   * program you would probably not use gst_bus_poll(), but rather set up an
   * async signal watch on the bus and run a main loop and connect to the
   * bus's signals to catch certain messages or all messages */
  msg = gst_bus_poll (bus, GST_MESSAGE_EOS | GST_MESSAGE_ERROR, -1);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS: {
      g_print ("EOS\n");
      break;
    }
    case GST_MESSAGE_ERROR: {
      GError *err = NULL; /* error to show to users                 */
      gchar *dbg = NULL;  /* additional debug string for developers */

      gst_message_parse_error (msg, &err, &dbg);
      if (err) {
        g_printerr ("ERROR: %s\n", err->message);
        g_error_free (err);
      }
      if (dbg) {
        g_printerr ("[Debug details: %s]\n", dbg);
        g_free (dbg);
      }
    }
    default:
      g_printerr ("Unexpected message of type %d", GST_MESSAGE_TYPE (msg));
      break;
  }
  gst_message_unref (msg);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_object_unref (bus);

}
