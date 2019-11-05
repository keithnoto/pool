#Authorship 

*	noto	2016-09-04

#Introduction

pool is a `std::thread` pool interface similar to Java's.  compile with `-std=gnu++11 -pthread` or similar.

#Notes

to update pool to use `pthread_t` instead of `std::thread`, you would:

```cpp
#include <pthread.h>

replace std::threads with pthread_t

replace void run_worker(worker_t*) with void* run_worker(void *)

cast (void*) to (worker_t*) in void* run_worker(void *)

remove question to joinable(), which applies to std::thread

replace call to join() thread with:

int rc = pthread_join( *(threads[w]), NULL );
if (rc) { throw DAU() << "failed to join thread " << w; }

replace new std::thread( run_worker, workers[n_started] ); with

int rc = pthread_create( threads[n_started], NULL, &run_worker, workers[n_started] );
if (rc) { throw DAU() << "failed to create thread " << n_started; }

```


This is the approximate diff in .h files:

```

	2c2
	<  * simple thread pool uses std::thread (compile with -std=gnu++11 -pthread or
	---
	>  * simple thread pool uses pthread (compile with -std=gnu++11 -pthread or
	16,18c16,18
	< #ifndef POOL_H
	< #define POOL_H
	< #include <thread>
	---
	> #ifndef POOL_H
	> #define POOL_H
	> #include <pthread.h>
	34c34
	<   friend void run_worker(worker_t*); // run_worker will set finished = true after running
	---
	>   friend void* run_worker(void *); // run_worker will set finished = true after running
	39c39
	< void run_worker(worker_t *worker);
	---
	> void* run_worker(void *worker);
	44c44
	< 	std::vector<std::thread*> threads;
	---
	> 	std::vector<pthread_t*> threads;
	54d53
	<


approximate diff in .cpp files:



	1a2
	> #include <cstdlib>
	2a4
	> #include "dau.h"
	6,8c8,11
	< void run_worker(worker_t *worker) {
	< 	worker->run();
	< 	worker->finished = true; 
	---
	> void* run_worker(void *worker) {
	> 	((worker_t*)worker)->run(); 
	> 	((worker_t*)worker)->finished = true; 
	> 	return worker;
	15,16c18,19
	< 	this->threads.push_back(NULL);
	< 	return workers.size(); 
	---
	> 	this->threads.push_back(NULL); // as many thread pointers as there are worker pointers
	> 	return ((int)(workers.size())); 
	33,34c36,40
	< 				if (threads[w] && workers[w] && workers[w]->finished && threads[w]->joinable()) {
	< 					threads[w]->join(); 
	---
	> 				if (threads[w] && workers[w] && workers[w]->finished) { 
	> 
	> 					int rc = pthread_join( *(threads[w]), NULL ); 
	> 					if (rc) { throw DAU() << "failed to join thread " << w; }
	> 
	36a43
	> 
	38a46
	> 
	40,43c48,50
	< 					if (callback) { 
	< 						callback( workers[w] ); 
	< 						workers[w] = NULL; //notox
	< 					}
	---
	> 
	> 					if (callback) { callback( workers[w] ); }
	> 					workers[w] = NULL;
	56c63,65
	< 			threads[n_started] = new std::thread( run_worker, workers[n_started] );  // freed when thread is finished, joined
	---
	> 			threads[n_started] = new pthread_t(); 
	> 			int rc = pthread_create( threads[n_started], NULL, &run_worker, workers[n_started] ); 
	> 			if (rc) { throw DAU() << "failed to create thread " << n_started; }

```



