// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

pthread_mutex_t lock_server::mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lock_server::cond = PTHREAD_COND_INITIALIZER;

lock_server::lock_server():
  nacquire (0)
{lockMap.clear();
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  pthread_mutex_lock(&mutex);

  if (lockMap.find(lid) == lockMap.end()){
    //find no such lock
    lock_status lockState = LOCKED;
    lockMap[lid] = lockState;
  }else{
    //find the lock but is locked
    while(lockMap[lid] == LOCKED)
      pthread_cond_wait(&cond, &mutex);//do we need to release the lock here?
    lockMap[lid] = LOCKED;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  pthread_mutex_lock(&mutex);
  if (lockMap.find(lid) == lockMap.end()){
    //find no such lock
    ret = lock_protocol::NOENT;
  }else{
    //release the lock
    lockMap[lid] = FREE;
    pthread_cond_signal(&cond);
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}
