// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
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
  r = lid;
  // A "coarse-granularity" mutex
  pthread_mutex_lock(&mutex);
  // If the lock has been created
  if (locks.find(lid) != locks.end()) {
    if (locks[lid] == FREE) {
      locks[lid] = LOCKED;
    }
    else {
      while (locks[lid] == LOCKED) {
        pthread_cond_wait(&cond, &mutex);
      }
      locks[lid] = LOCKED;
    }
  }
  // If the lock hasn't been created
  else {
    locks[lid] = LOCKED;
  }
  printf("ls: acq get here, id: %lld\n", lid);
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab4 code goes here
  r = lid;
  pthread_mutex_lock(&mutex);
  if (locks.find(lid) != locks.end()) {
    locks[lid] = FREE;
  }
  else {
    ret = lock_protocol::NOENT;
  }
  printf("ls: rel get here, id: %lld\n", lid);
  pthread_mutex_unlock(&mutex);
	pthread_cond_signal(&cond);
  return ret;
}
