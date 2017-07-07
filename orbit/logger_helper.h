#ifndef LOGGER_HELPER_H__
#define LOGGER_HELPER_H__

#include <stdlib.h>
#include <stdio.h>

#include "glog/logging.h"

#define ELOG_MAX_BUFFER_SIZE 30000

#define SPRINTF_ELOG_MSG(buffer, fmt, args...) \
    char buffer[ELOG_MAX_BUFFER_SIZE]; \
    snprintf( buffer, ELOG_MAX_BUFFER_SIZE, fmt, ##args );

#define ELOG_DEBUG2(fmt, args...) \
  {                               \
    SPRINTF_ELOG_MSG( __tmp, fmt, ##args );   \
    VLOG(3) << __tmp;                         \
  }                               \

#define ELOG_INFO2(fmt, args...) \
  {                               \
  SPRINTF_ELOG_MSG( __tmp, fmt, ##args );       \
  LOG(INFO) << __tmp;                           \
  }                               \

#define ELOG_WARN2(fmt, args...) \
  {                               \
  SPRINTF_ELOG_MSG( __tmp, fmt, ##args );       \
  LOG(WARNING) << __tmp;                           \
  }                               \

#define ELOG_ERROR2(fmt, args...) \
  {                               \
  SPRINTF_ELOG_MSG( __tmp, fmt, ##args ); \
  LOG(ERROR) << __tmp;                    \
  }                               \

#define ELOG_FATAL2(fmt, args...) \
  {                               \
  SPRINTF_ELOG_MSG( __tmp, fmt, ##args ); \
  LOG(FATAL) <<  __tmp;                   \
  }                               \

#define ELOG_DEBUG(fmt, args...) \
    ELOG_DEBUG2(fmt, ##args );

#define ELOG_INFO(fmt, args...) \
    ELOG_INFO2(fmt, ##args );

#define ELOG_WARN(fmt, args...) \
    ELOG_WARN2(fmt, ##args );

#define ELOG_ERROR(fmt, args...) \
    ELOG_ERROR2(fmt, ##args );

#define ELOG_FATAL(fmt, args...) \
    ELOG_FATAL2(fmt, ##args );

#endif  /* __ELOG_H__ */
