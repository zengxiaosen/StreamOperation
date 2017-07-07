// Copyright 2015 All Rights Reserved.
// Author: eric@orangelab.cn (Cheng Xu)

#include "gst_util.h"
#include <gstreamer-1.5/gst/gst.h>

namespace olive {
  void InitGstreamer(int argc, char *argv[]) {
    /* Initialize GStreamer */
    gst_init (&argc, &argv);
  }
}


