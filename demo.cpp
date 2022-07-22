// SYNOPSIS "example multi-threaded program that uses pool"
// VERSION  "1"

#include <sys/timeb.h>
#include <iostream>
#include "pool.h"

// a worker that factors a number for primality in the stupidest possible way
class primer_t : public noto::worker_t {
  private:
  	bool prime; // set this during run()
  public:
	const unsigned long int n; // number to test for primality
	primer_t(unsigned long int an) : n(an) { this->priority = an; }
	void run(); // compute if n is prime
	void finalize(); // print the answer
};

// get wall clock time now
inline double now() { struct timeb tp; ftime(&tp); return tp.time + 1e-3 * tp.millitm; }

// compute if this->n is prime, store result in this->prime bool
void primer_t::run() {

	this->prime = true;
	for (unsigned long int d=2; d<n; d++) { 
		if ( n % d == 0 ) { 
			this->prime = false;
		} 
	}

}

// print result to screen
void primer_t::finalize() { 
	if (this->prime) { 
		std::cout << n << " is prime" << std::endl;
	}
}

int main( int argc, const char **argv ) {

	if (argc != 2) { 
		std::cerr << std::endl << "Usage: " << argv[0] << " <number of threads>" << std::endl << std::endl;
		return 1;
	}

	const size_t threadcount = (size_t)(atoi(argv[1])); 

	const double start_time = now();

	noto::pool_t pool( threadcount );  // create pool

	std::thread *asynchronous_pool_start_thread = pool.start_asynchronously(noto::delete_worker); // tell pool to delete each worker after it completes

	// add some work, these will start immediately
	for (size_t n=100000001; n<=100000101; n++) { pool.add(new primer_t(n)); }
	for (size_t n=2; n<=100; n++) { pool.add(new primer_t(n)); }
	for (size_t n=10000001; n<=10000101; n++) { pool.add(new primer_t(n)); }
	for (size_t n=1000001; n<=1000101; n++) { pool.add(new primer_t(n)); }
	for (size_t n=100001; n<=100101; n++) { pool.add(new primer_t(n)); }

	pool.finish(); // no more workers will be added, pool knows to stop when last thread finishes
	asynchronous_pool_start_thread->join(); // wait for that...
	
	const double end_time = now();

	std::cout << "all done, " << (end_time - start_time) << " seconds" << std::endl;

	return 0; 

}
