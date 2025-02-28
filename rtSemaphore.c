#include "rtSemaphore.h"
#include "rtLog.h"
#include "rtError.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define ERROR_CHECK(CMD) \
{ \
  int err; \
  if((err=CMD) != 0) \
  { \
    rtLog_Error("Error %d:%s running command " #CMD, err, strerror(err)); \
    rc = RT_ERROR; \
  } \
}

struct _rtSemaphore
{
  pthread_mutex_t m;
  pthread_cond_t c;
  int v;
};

rtError rtSemaphore_Create(rtSemaphore* sem)
{
  int rc = 0;
  pthread_mutexattr_t mattrib;
  pthread_condattr_t cattrib;
  *sem = malloc(sizeof(struct _rtSemaphore));
  (*sem)->v = 0;
  ERROR_CHECK(pthread_mutexattr_init(&mattrib));
  ERROR_CHECK(pthread_mutexattr_settype(&mattrib, PTHREAD_MUTEX_ERRORCHECK));
  ERROR_CHECK(pthread_mutex_init(&(*sem)->m, &mattrib));  
  ERROR_CHECK(pthread_mutexattr_destroy(&mattrib));
  ERROR_CHECK(pthread_condattr_init(&cattrib));
  ERROR_CHECK(pthread_condattr_setclock(&cattrib, CLOCK_MONOTONIC));
  ERROR_CHECK(pthread_cond_init(&(*sem)->c, &cattrib));
  ERROR_CHECK(pthread_condattr_destroy(&cattrib));
  return rc;
}

rtError rtSemaphore_Destroy(rtSemaphore sem)
{
  int rc = RT_OK;
  ERROR_CHECK(pthread_cond_signal(&sem->c));
  ERROR_CHECK(pthread_mutex_destroy(&sem->m));
  ERROR_CHECK(pthread_cond_destroy(&sem->c));
  free(sem);
  return rc;
}

rtError rtSemaphore_Post(rtSemaphore sem)
{
  int rc = RT_OK;
  ERROR_CHECK(pthread_mutex_lock(&sem->m));
  sem->v++;
  ERROR_CHECK(pthread_mutex_unlock(&sem->m));
  ERROR_CHECK(pthread_cond_signal(&sem->c));
  return rc;
}

rtError rtSemaphore_GetValue(rtSemaphore sem, int* val)
{
  int rc = RT_OK;
  *val = sem->v;
  return rc;
}

rtError rtSemaphore_Wait(rtSemaphore sem)
{
    return rtSemaphore_TimedWait(sem, NULL);
}

rtError rtSemaphore_TimedWait(rtSemaphore sem, struct timespec* t)
{
  int rc = RT_OK;
  int err;
  ERROR_CHECK(pthread_mutex_lock(&sem->m));
  if(sem->v > 0)
  {
    sem->v--;
    ERROR_CHECK(pthread_mutex_unlock(&sem->m));
    return RT_OK;
  }
  if(t)
    err = pthread_cond_timedwait(&sem->c, &sem->m, t);
  else
    err = pthread_cond_wait(&sem->c, &sem->m);
  if(err == 0)
  {
    if(sem->v > 0)
      sem->v--;
  }
  else
  {
    if(t && err == ETIMEDOUT)
    {
      rc = RT_ERROR_TIMEOUT;
    }
    else if(err != 0)
    {
      rtLog_Error("Error %d:%s running command pthread_cond_timedwait", err, strerror(err));
      rc = RT_ERROR;
    }
  }
  ERROR_CHECK(pthread_mutex_unlock(&sem->m));
  return rc;
}
