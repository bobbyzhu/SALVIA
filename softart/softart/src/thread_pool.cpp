#include "eflib/include/eflib.h"

#include "../include/cpuinfo.h"
#include "../include/thread_pool.h"

boost::threadpool::pool& global_thread_pool()
{
	static boost::threadpool::pool tp(num_cpu_cores());
	return tp;
}
