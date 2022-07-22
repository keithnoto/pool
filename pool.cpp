#include "pool.h"
#include <unistd.h>
#include <cassert>
#include <exception>

namespace noto {

// run worker, then set finished to true
void run_worker(worker_t *worker) {
	worker->run(); 
	worker->finished = true; 
}

// compare for priority
bool worker_cmp_t::operator()(const worker_sn_t &a, const worker_sn_t &b) const {
	return (a.worker->priority == b.worker->priority)
			? a.serial_number > b.serial_number
			: a.worker->priority > b.worker->priority;
}

// add worker to list (to be called before execute)
// run immediately (and execute callback) iff threadcount is zero
int pool_t::add(worker_t *worker, void (*callback)(worker_t*)) {
	int result = -1; 
	if (this->threadcount) { 
		this->mutex.lock();	
		if (all_aboard) {
			mutex.unlock();
			throw std::runtime_error("worker added to pool after it closed");
		}
		worker->callback = callback; // assign callback
		worker->finished = false;
		worker_sn_t sn; 
		sn.serial_number = queue.size() + workers.size(); 
		sn.worker = worker;
		this->queue.push(sn); 
		this->mutex.unlock();	
		result = sn.serial_number;
	} else {
		worker->finished = false;
		worker->run(); 
		worker->finalize();
		worker->finished = true;
		if (callback) { callback(worker); }
		this->mutex.lock();	
		this->workers.push_back(NULL); // placeholder
		this->n_finished++;
		this->mutex.unlock();	
	}
	return result;

}

// execute all workers, with no more than threadcount running concurrently
void pool_t::start(void (*callback)(worker_t*)) {

	// loop until: finished AND empty queue AND all finished 
	while (!(all_aboard && queue.empty() && n_finished == workers.size())) {

		// loop while something is running but there's nothing to add
		while ( n_running && ( (n_running >= this->threadcount) || (queue.empty() && n_running) ) ) {
		
			size_t prev_n_running = n_running;
			for (size_t w = 0; w < n_started; w++) {
				if (threads[w] && workers[w] && workers[w]->finished && threads[w]->joinable()) {
					threads[w]->join(); 
					delete threads[w]; 
					threads[w] = NULL; 
					n_finished++;
					n_running--;
					workers[w]->finalize();
					if (workers[w]->callback) { 
						// if worker's callback is not NULL, it has priority
						(workers[w]->callback)(workers[w]);
					} else if (callback) { 
						// otherwise, use the one given here.
						callback(workers[w]); 
					}
					workers[w] = NULL;
					break;
				}
			}
			if (prev_n_running == n_running) {
				usleep(timeout_us);
			}
		}

		// loop while there are jobs to start
		while ((n_running < this->threadcount) && queue.size()) {
			// pop next one off the queue
			this->mutex.lock(); 
			assert(n_started == workers.size()); 
			worker_sn_t sn = queue.top(); 
			queue.pop();
			workers.push_back(sn.worker); 
			threads.push_back(NULL); 
			this->mutex.unlock(); 
			threads[n_started] = new std::thread(run_worker, workers[n_started]);
			n_started++;
			n_running++;
		} // add another worker to the running list?
	
		// whether we sleep above or not, 
		// we may be done with all submitted jobs, but not yet received a call to 'finish'
		usleep(timeout_us);
	
	} // while not all jobs finished

} // pool_t::start

// start a pool with the given callback
void start_pool( pool_t *pool, void (*callback)(worker_t*) ) {
	pool->start(callback); 
}

// start the pool in a separate thread (and return a pointer to it)
// caller must join the thread after pool::finish has been called
std::thread*
pool_t::start_asynchronously(void (*callback)(worker_t*)) {
	return new std::thread(start_pool, this, callback); 
}

}//namespace
