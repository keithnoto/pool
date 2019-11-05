#include <unistd.h>
#include <cassert>
#include <exception>
#include "pool.h"

namespace noto {

// run worker, then set finished to true
void run_worker(worker_t *worker) {
	worker->run(); 
	worker->finished = true; 
}

bool worker_cmp_t::operator()(const worker_sn_t &a, const worker_sn_t &b) const {
	return (a.worker->priority == b.worker->priority)
			? a.serial_number > b.serial_number
			: a.worker->priority > b.worker->priority;
}

// add worker to list (to be called before execute)
int pool_t::add(worker_t *worker) {
	this->mutex.lock();	
	if (all_aboard)
	{
		mutex.unlock();
		throw std::runtime_error("worker added to pool after it closed");
	}
	worker->finished = false;
	worker_sn_t sn; 
	sn.serial_number = queue.size() + workers.size(); 
	sn.worker = worker;
	this->queue.push(sn); 
	this->mutex.unlock();	
	return sn.serial_number;
}

// execute all workers, with no more than threadcount running concurrently
void pool_t::start(void (*callback)(worker_t*)) {
	size_t n_running = 0;
	size_t n_finished = 0;
	size_t n_started = 0;
	while (!(all_aboard && queue.empty() && n_finished == workers.size())) {
		while ((n_running >= this->threadcount) || (queue.empty() && n_running)) {
			size_t prev_n_running = n_running;
			for (size_t w = 0; w < n_started; w++) {
				if (threads[w] && workers[w] && workers[w]->finished && threads[w]->joinable()) {
					threads[w]->join(); 
					delete threads[w]; 
					threads[w] = NULL; 
					n_finished++;
					n_running--;
					workers[w]->finalize(); 
					if (callback) { 
						callback(workers[w]); 
						workers[w] = NULL;
					}
					break;
				}
			}
			if (prev_n_running == n_running) {
				usleep(timeout_us);
			}
		}
		while (n_running < this->threadcount && queue.size()) { 
			// pop next one off the queue
			this->mutex.lock(); 
			assert(n_started == workers.size()); 
			worker_sn_t sn = queue.top(); 
			queue.pop();
			workers.push_back(sn.worker); 
			threads.push_back(NULL); 
			this->mutex.unlock(); 
			usleep(timeout_us);
			threads[n_started] = new std::thread(run_worker, workers[n_started]);
			n_started++;
			n_running++;
		}
	}
}

}//namespace
