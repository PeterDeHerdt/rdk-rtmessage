/*
##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2019 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
# This file contains material from pxCore which is Copyright 2005-2018 John Robinson
# Licensed under the Apache-2.0 license.
*/

// rtLog.cpp

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include "rtLog.h"

#ifndef WIN32
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#else
#include <Windows.h>
#define strcasecmp _stricmp
#endif

#ifndef RT_LOGPREFIX
#define RT_LOGPREFIX "rt:"
#endif

#ifdef ENABLE_RDKLOGGER
#include "rdk_debug.h"
#endif

#include <inttypes.h>

#ifdef __cplusplus
static void setLogLevelFromEnvironment()
#else
static void setLogLevelFromEnvironment() __attribute__((constructor));

void setLogLevelFromEnvironment()
#endif
{
  const char* s = getenv("RT_LOG_LEVEL");
  if (s && strlen(s))
  {
    rtLogLevel level = RT_LOG_ERROR;
    if      (strcasecmp(s, "debug") == 0) level = RT_LOG_DEBUG;
    else if (strcasecmp(s, "info") == 0)  level = RT_LOG_INFO;
    else if (strcasecmp(s, "warn") == 0)  level = RT_LOG_WARN;
    else if (strcasecmp(s, "error") == 0) level = RT_LOG_ERROR;
    else if (strcasecmp(s, "fatal") == 0) level = RT_LOG_FATAL;
    else
    {
      fprintf(stderr, "invalid RT_LOG_LEVEL set: %s", s);
      abort();
    }
    rtLog_SetLevel(level);
  }
}

#ifdef __cplusplus
struct LogLevelSetter
{
  LogLevelSetter()
  {
    setLogLevelFromEnvironment();
  }
};

static LogLevelSetter __logLevelSetter; // force RT_LOG_LEVEL to be read from env
#endif

static const char* rtLogLevelStrings[] =
{
  "DEBUG",
  "INFO",
  "WARN",
  "ERROR",
  "FATAL"
};

static rtLogHandler sLogHandler = NULL;
static rtLogLevel sLevel = RT_LOG_FATAL;
static rtLoggerSelection sOption =
#ifdef ENABLE_RDKLOGGER
  RT_USE_RDKLOGGER;
#else
  RT_USE_RTLOGGER;
#endif

static const uint32_t numRTLogLevels = sizeof(rtLogLevelStrings)/sizeof(rtLogLevelStrings[0]);

rtThreadId rtThreadGetCurrentId();

const char* rtLogLevelToString(rtLogLevel l)
{
  const char* s = "OUT-OF-BOUNDS";
  if (l < numRTLogLevels)
    s = rtLogLevelStrings[l];
  return s;
}

rtLogLevel rtLogLevelFromString(char const* s)
{
  rtLogLevel level = sLevel;
  if (s)
  {
    if (strcasecmp(s, "debug") == 0)
      level = RT_LOG_DEBUG;
    else if (strcasecmp(s, "info") == 0)
      level = RT_LOG_INFO;
    else if (strcasecmp(s, "warn") == 0)
      level = RT_LOG_WARN;
    else if (strcasecmp(s, "error") == 0)
      level = RT_LOG_ERROR;
    else if (strcasecmp(s, "fatal") == 0)
      level = RT_LOG_FATAL;
  }
  return level;
}

static const char* rtTrimPath(const char* s)
{
  if (!s)
    return s;

  const char* t = strrchr(s, (int) '/');
  if (t) t++;
  if (!t) t = s;

  return t;
}


void rtLogSetLogHandler(rtLogHandler logHandler)
{
  sLogHandler = logHandler;
}

rtLogHandler rtLogGetLogHandler()
{
  return sLogHandler;
}

void rtLog_SetLevel(rtLogLevel level)
{
  sLevel = level;
}

rtLogLevel rtLog_GetLevel()
{
  return sLevel;
}

void rtLog_SetOption(rtLoggerSelection option)
{
  sOption = option;
}

rtLoggerSelection rtLog_GetOption()
{
  return sOption;
}

#define RT_LOG_BUFFER_SIZE    1024
void rtLogPrintf(rtLogLevel level, const char* file, int line, const char* format, ...)
{
  if (level < sLevel)
    return;

  size_t n = 0;
  char buff[RT_LOG_BUFFER_SIZE] = {0};

  const char* path = rtTrimPath(file);

  rtThreadId threadId = rtThreadGetCurrentId();

  va_list args;

  va_start(args, format);
  n = vsnprintf(buff, (RT_LOG_BUFFER_SIZE - 1), format, args);
  va_end(args);

  if (n > (RT_LOG_BUFFER_SIZE - 1))
  {
    buff[RT_LOG_BUFFER_SIZE - 4] = '.';
    buff[RT_LOG_BUFFER_SIZE - 3] = '.';
    buff[RT_LOG_BUFFER_SIZE - 2] = '.';
  }
  buff[RT_LOG_BUFFER_SIZE - 1] = '\0';

  if (NULL != sLogHandler)
  {
    sLogHandler(level, path, line, threadId, buff);
  }
  else
  {
    struct timeval tv;
    struct tm* lt;
    gettimeofday(&tv, NULL);
    lt = localtime(&tv.tv_sec);

    printf("%.2d:%.2d:%.2d.%.3ld %5s %s:%d -- Thread-%" RT_THREADID_FMT ": %s \n",
        lt->tm_hour, lt->tm_min, lt->tm_sec, tv.tv_usec / 1000,
        rtLogLevelToString(level), path, line, threadId, buff);
  }
}

rtThreadId rtThreadGetCurrentId()
{
#ifdef __APPLE__
  uint64_t threadId = 0;
  pthread_threadid_np(NULL, &threadId);
  return threadId;
#elif WIN32
  return GetCurrentThreadId();
#else
  return syscall(__NR_gettid);
#endif
}
