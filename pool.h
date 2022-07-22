/*
 * simple thread pool uses std::thread 
 * (compile with -std=gnu++11 -pthread or similar)

 * To use, create objects that subclass worker_t and override the void run()
 * and optionally the void finalize() methods.  Create a pool_t, supplying the
 * number of max concurrent threads to the constructor.  Add each worker to the
 * pool_t using pool_t::add(worker_t*), then call the pool_t's execute().  Each
 * worker will run, and when it's finished it's finalize() method will be
 * called in a threadsafe way (i.e., only one finalize() is run at a time)

 * It is possible to add workers after the start of execution.  To do this, use
 * start() instead of execute() to begin execution.  at some point after that,
 * call finish() to indicate that no more workers will be added after that
 * point (so the pool knows when it is truly done executing).  any call to
 * add() after finish() will throw an exception.  note that start() will
 * control the thread pool until all threads finish, so finish() must be called
 * by some concurrent thread.  start_asynchronously() is useful here.  that
 * function returns a thread that you can join() when you are ready.
 
 * 2016-Aug-22
 * 2018-Apr-19 -- added start(),finish() -- ability to add workers after execution starts
 * 2018-May-17 -- change to priority queue for unstarted workers
 * 2020-Oct-31 -- add delete_worker function to this namespace, added alternative pool_t::add (the one with callback) to execute immediately if threadcount=0
 * 2022-Feb-11 -- updated behavior to be reasonable if threadcount is zero: just run the threads as they are added
 */

#ifndef POOL_H
#define POOL_H

#include <thread>
#include <queue>
#include <vector>
#include <mutex>

namespace noto {

class worker_t { 

  private:
  	bool finished = false; // set to true by pool_t calling run_worker
	void (*callback)(worker_t*) = NULL; // execute this after finalize

  protected:
    size_t priority = 0; // can be changed by subclass, lower priority value gets run first (ties broken by order added to pool)

  public:
	virtual void run() = 0;	// user will override with task (similar to java)
	virtual void finalize() { }; // user may optionally override (e.g., to log status) only one finalize method runs at a time
	virtual ~worker_t() { }

  friend class pool_t;	// pool_t will access finished status
  friend class worker_cmp_t;  // will access priority
  friend void run_worker(worker_t*); // run_worker will set finished = true after running

};

// delete a worker (can be passed to pool::execute)
inline void delete_worker( worker_t *worker ) { delete worker; }

// run the worker and set its 'finished' to true
void run_worker(worker_t *worker);

// a worker and a serial number which keeps the order a worker was added to a queue
struct worker_sn_t { 
	worker_t *worker;
	size_t serial_number; 
};

// compare two queue items for priority
class worker_cmp_t { 
  public:
  	bool operator()(const worker_sn_t &a, const worker_sn_t &b) const;
}; 


// collection of workers to run some at a time
class pool_t { 
  protected:
	std::vector<std::thread*> threads; // list of threads I've started
	std::vector<worker_t*> workers; // list of started workers
	std::priority_queue<worker_sn_t,std::vector<worker_sn_t>,worker_cmp_t> queue; // queue of not-yet-started workers
	std::mutex mutex; // for synchronous operations
	bool all_aboard = false; // have all workers been added to the queue (i.e., do I know I'm done when the queue is empty?)
	size_t n_running = 0; // number of threads currently running (see start function)
	size_t n_finished = 0; 
	size_t n_started = 0;

  public:
  	size_t threadcount; // it is legal to change the threadcount while the pool is executing
  	const int timeout_us;	// microsecond timeout when no running thread can be started or finalized

	pool_t(size_t c) : threadcount(c), timeout_us(1000) { }
	pool_t(size_t c, int us) : threadcount(c), timeout_us(us) { }

	// add one more worker, or run immediately (and do callback) if and only if threadcount is zero.  
	int add(worker_t *worker, void (*callback)(worker_t*)=NULL); 
	
	// run all workers, no more than threadcount concurrently, but more may be
	// added until `finish' is called.  if callback is not NULL, it will be
	// applied to all workers whose callback is NULL (i.e., worker callback
	// overrides pool::start callback).  the callback argument is for backward
	// compatability--better to have each worker have its own
	void start(void (*callback)(worker_t*)=NULL); 

	// start pool in a thread (that you must join after the this pool's call to finish).  
	// note that this thread runs separate from the pool's workers
	// (and may use a bit of processing outside that of the pool's workers)
  	std::thread* start_asynchronously(void (*callback)(worker_t*)=NULL); 

	// tell pool no more workers will be added, and `start` can complete when
	// there are no more unfinished workers in the queue
	void finish() { all_aboard = true; } 

	// just run all workers, apply callback to all workers whose callback is NULL
  	void execute(void (*callback)(worker_t*)=NULL) { finish(); start(callback); } 

  	size_t pending() const { return queue.size(); } // number of workers queued 
	size_t running() const { return n_running; } // number currently running not confirmed finished
	size_t finished() const { return n_finished; } // number finished
};

}//namespace
#endif
