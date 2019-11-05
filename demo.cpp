const char SYNOPSIS[] = "example multi-threaded program that uses pool"; 
const char VERSION[] = "1";

#include <sys/timeb.h>
#include <iostream>
#include "pool.h"
using namespace noto;

// get wall clock time now
inline double now() { struct timeb tp; ftime(&tp); return tp.time + 1e-3 * tp.millitm; }

// adderpool will call finish() after `groups' calls to groupinc
class adderpool_t : public pool_t {
  private:
  	const int groups;
	int incs = 0;
  public: 
  	adderpool_t(int threadcount, int g) : pool_t(threadcount),groups(g) { }
	void groupinc() { 
		incs++; 
		if (incs >= groups) { 
			this->finish(); 
		} 
	}
};

// delete dynamically allocated data
void delete_worker( worker_t *worker ) {
	delete worker;
}

// a worker that factors a number for primality in the stupidest possible way
class primer_t : public worker_t {
  private:
  	bool prime = true;  // set to false if I find out the number isn't prime
	double t0,t1; // start,end wall clock time
  public:
	const unsigned long int n; // number to test for primality
	primer_t(unsigned long int an) : n(an) { priority = an; }
	void run(); // compute if n is prime
	void finalize(); // print the answer
};


void primer_t::run() { 

	this->t0 = now();
	for (unsigned long int d=2; d<n; d++) { 
		if ( n % d == 0 ) { 
			this->prime = false;
		} 
	}
	this->t1 = now();

}

void primer_t::finalize() { 
	
	std::cout << n << " is " << (prime?"":"not ") << "prime (" << (t1-t0) << " seconds)" << std::endl;

}

// adder worker takes a range of numbers and adds primality test workers for
// each to its thread pool
class adder_t : public worker_t { 
  private:
  	adderpool_t *pool;
  	const unsigned long int min;
  	const unsigned long int max;
  public:
	adder_t(adderpool_t *p, unsigned long int a, unsigned long int b) : pool(p),min(a),max(b) { priority = 0; }
	void run(); // add workers
	void finalize(); // call groupinc
}; 

void adder_t::run() { 
	for (unsigned long int n=min; n<=max; n++) { 
		this->pool->add( new primer_t( n ) ); 
	}
}

void adder_t::finalize() { 
	this->pool->groupinc(); 
}

int main( int argc, const char **argv ) {

	if (argc != 2) { 
		std::cerr << std::endl << "Usage: " << argv[0] << " <number of threads>" << std::endl << std::endl;
		return 1;
	}

	const size_t threadcount = (size_t)(atoi(argv[1]));

	std::cout << "create thread pool size " << threadcount << " ..." << std::endl;
	adderpool_t pool(threadcount, 5); // there will be 5 adder_t 

	std::cout << "add some work ..." << std::endl;

	// add the 5 adder_t
	pool.add(new adder_t(&pool, 100000001,100000100)); 
	pool.add(new adder_t(&pool, 2,100)); 
	pool.add(new adder_t(&pool, 10000001,10000100)); 
	pool.add(new adder_t(&pool, 1000001,1000100)); 
	pool.add(new adder_t(&pool, 100001,100100)); 

	std::cout << "execute!" << std::endl;

	// start running the adder_t workers, which will add other prime_t workers
	// to the pool.  once the 5th adder_t is done, the pool will finish() and
	// start() will return control to this function when all the prime_t
	// workers are done.
	
	pool.start(delete_worker);  

	// ... and they're done 

	std::cout << "all done!" << std::endl;

	return 0; 

}
