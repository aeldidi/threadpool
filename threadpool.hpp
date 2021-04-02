// SPDX-License-Identifier: BlueOak-1.0.0
// To use, in one of the source files, define THREADPOOL_IMPLEMENTATION

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace didi
{

class tp_thread;
class threadpool_queue;

class _threadpool
{
public:
	int                     threads_alive;
	int                     num_threads;
	int                     num_threads_working;
	std::mutex              count_lock;
	std::condition_variable all_idle;
	std::vector<tp_thread>  threads;
	threadpool_queue*       queue;
	_threadpool(int threadc);
	~_threadpool();
	void reset();
	void wait();
	void add_job(std::function<void()> fn);
};

class threadpool
{
private:
	_threadpool* tp;

public:
	threadpool(int threadc);
	~threadpool();
	void reset();
	void wait();
	void add_job(std::function<void()> fn);
};

} // namespace didi

#endif

// ======== IMPLEMENTATION ======== //

#ifdef THREADPOOL_IMPLEMENTATION

namespace didi
{

// ======== API ======== //

// Creates a threadpool with threadc threads
threadpool::threadpool(int threadc)
{
	tp = new _threadpool(threadc);
}

// reset()s the threadpool before freeing it's resources
threadpool::~threadpool()
{
	delete tp;
}

// Clear's the threadpool's queue and calls threadpool::wait()
void
threadpool::reset()
{
	tp->reset();
}

// wait()s for the threadpool to finish all queued jobs
void
threadpool::wait()
{
	tp->wait();
}

// Adds job fn to the head of the threadpool's queue
void
threadpool::add_job(std::function<void()> fn)
{
	tp->add_job(fn);
}

// ======== Internal Structures ======== //

class threadpool_job
{
public:
	std::function<void()> function;
	threadpool_job*       next;
	threadpool_job()
	{
		next = nullptr;
	};
};

class threadpool_queue
{
public:
	int                     length;
	std::mutex              lock;
	std::condition_variable not_empty;
	threadpool_job*         head;
	threadpool_queue()
	{
		length = 0;
		head   = nullptr;
	};
	// Recursively frees all of the jobs in the threadpool's queue.
	// Assumes lock on queue is NOT held.
	// Returns on NULL.
	void clear()
	{
		threadpool_job* first = nullptr;
		lock.lock();
		if (head == nullptr) {
			return;
		}

		first = head;
		if (first->next == nullptr) {
			delete first;
			return;
		}
		destroy_all_jobs(first->next);
		head   = nullptr;
		length = 0;
		lock.unlock();
	}
	// Recursively frees all connected threadpool_jobs. Returns on NULL.
	// Assumes caller has a lock on the queue.
	void destroy_all_jobs(threadpool_job* jb)
	{
		if (jb == nullptr) {
			return;
		}

		if (jb->next != nullptr) {
			destroy_all_jobs(jb->next);
			delete jb;
			return;
		}
	}
	// pushes a job to the end of the queue list
	void push(threadpool_job* jb)
	{
		if (jb == nullptr) {
			return;
		}

		if (head == nullptr) {
			head = jb;
			return;
		}

		jb->next = head;
		head     = jb;
	}
};

// a wrapper around std::thread with a pointer to it's associated threadpool
class tp_thread
{
public:
	std::thread  thread;
	_threadpool* pool;
};

// the work function for all the threads
static void*
thread_work_function(_threadpool* tp)
{
	threadpool_job*       current = nullptr;
	std::function<void()> fn;

	tp->count_lock.lock();
	tp->num_threads++;
	tp->count_lock.unlock();

	while (tp->threads_alive) {
		threadpool_queue* queue = tp->queue;

		// wait for new jobs to be availible
		{
			std::unique_lock<std::mutex> lck(queue->lock);
			while (queue->length == 0 && tp->threads_alive) {
				queue->not_empty.wait(lck);
			}
		}

		// check if the thread is being killed
		if (!tp->threads_alive) {
			break;
		}

		// since the thread isn't being killed, set it as working
		tp->count_lock.lock();
		tp->num_threads_working++;
		tp->count_lock.unlock();

		// get the next job
		queue->lock.lock();
		current = queue->head;

		switch (queue->length) {
		case 0:
			break;
		case 1:
			queue->head   = nullptr;
			queue->length = 0;
			break;
		default:
			queue->head = current->next;
			queue->length--;
			queue->not_empty.notify_one();
		}

		queue->lock.unlock();

		// if there is no current job, we want to continue waiting
		if (current == nullptr) {
			tp->count_lock.lock();
			tp->num_threads_working--;
			if (tp->num_threads_working == 0) {
				tp->all_idle.notify_one();
			}
			tp->count_lock.unlock();
			continue;
		}

		fn = current->function;

		// does the work and disposes of the function's memory
		fn();
		delete current;

		// since it's done, tell the pool it's idle
		tp->count_lock.lock();
		tp->num_threads_working--;
		if (tp->num_threads_working == 0) {
			tp->all_idle.notify_one();
		}
		tp->count_lock.unlock();
	}
	tp->count_lock.lock();
	tp->num_threads--;
	tp->count_lock.unlock();

	return nullptr;
}

_threadpool::_threadpool(int threadc)
{
	if (threadc < 0) {
		throw std::invalid_argument("threadc may not be less than 1");
	}

	// ==== threadpool Initialization ==== /

	threads_alive       = 1;
	num_threads         = 0;
	num_threads_working = 0;

	// ==== Queue Initialization ==== //

	queue = new threadpool_queue;

	// ==== Thread Initialization ==== //

	for (int i = 0; i < threadc; i++) {
		threads.push_back(tp_thread{});
		threads[i].thread = std::thread(thread_work_function, this);
		threads[i].thread.detach();
	}

	// wait for all the threads to spin up
	while (num_threads != threadc) {
		std::this_thread::yield();
	}
}

_threadpool::~_threadpool()
{
	int threadc;

	// calling this function frees all of the jobs and calls this.wait()
	this->reset();

	// signals the end of all the threads' loops
	threads_alive = 0;

	this->count_lock.lock();
	threadc = num_threads;
	for (int i = 0; i < threadc; i++) {
		num_threads--;
	}
	this->count_lock.unlock();

	// make sure nothing's still waiting for any conditions
	this->queue->not_empty.notify_all();
	this->all_idle.notify_all();

	delete this->queue;
}

void
_threadpool::reset()
{
	// clears the queue so no new jobs can be added
	queue->clear();
	// waits for the current jobs to finish
	this->wait();
}

// waits for threadpool to finish and go idle
void
_threadpool::wait()
{
	{
		std::unique_lock<std::mutex> lck(count_lock);
		while (queue->length || num_threads_working) {
			all_idle.wait(lck);
		}
	}
}

// Adds the function fn to the threadpool's jobqueue.
void
_threadpool::add_job(std::function<void()> fn)
{
	threadpool_job* job = new threadpool_job;
	queue->lock.lock();

	job->function = fn;
	job->next     = nullptr;

	if (queue->length == 0) {
		queue->head = job;
	} else {
		queue->push(job);
	}
	queue->length++;
	queue->not_empty.notify_one();
	queue->lock.unlock();
}

} // namespace didi

#endif // THREADPOOL_IMPLEMENTATION
