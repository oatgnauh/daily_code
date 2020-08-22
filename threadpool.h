#pragma once
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>



class SingleThreadUnit {
public:
	SingleThreadUnit();
	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		->std::future<typename std::result_of<F(Args...)>::type>;
	~SingleThreadUnit();

	std::thread::id GetThreadId() const {
		return tid_;
	}

	uint32_t GetTaskSize() const {
		return tasks.size();
	}

	struct Task
	{
		int ownner_;	//识别这个
		std::function<void()> task_;
	};
private:
	// need to keep track of threads so we can join them
	//std::vector< std::thread > workers;
	std::thread thread_;
	// the task queue
	std::queue<Task> tasks;

	// synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
	std::thread::id tid_;
};

// the constructor just launches some amount of workers
inline SingleThreadUnit::SingleThreadUnit()
	: stop(false)
{
	thread_ = std::thread(
		[this]
	{
		tid_ = std::this_thread::get_id();
		for (;;)
		{
			std::function<void()> task;

			{
				std::unique_lock<std::mutex> lock(this->queue_mutex);
				this->condition.wait(lock,
					[this] { return this->stop || !this->tasks.empty(); });
				if (this->stop && this->tasks.empty())
					return;
				task = std::move(this->tasks.front().task_);
				this->tasks.pop();
			}

			task();
		}
	}
	);
}

// add new work item to the pool
template<class F, class... Args>
auto SingleThreadUnit::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
	using return_type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		// don't allow enqueueing after stopping the pool
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() { (*task)(); });
	}
	condition.notify_one();
	return res;
}

// the destructor joins all threads
inline SingleThreadUnit::~SingleThreadUnit()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();
	thread_.join();
}


class ThreadPool
{
public:
	ThreadPool(uint16_t size)
	{
		for (int i = 0; i < size; i++)
		{
			workers_.push_back(new SingleThreadUnit());
		}
		
	}
	~ThreadPool()
	{
		for (auto worker : workers_)
		{
			delete worker;
		}
	}

	//新的对象产生的时候才做负载均衡， 已存在的对象post进来的任务要哈希到同一线程
	std::thread::id PostTask_RoundRobin(std::function<void()> func)
	{
		workers_[current_round_]->enqueue(func);
		if (current_round_ == workers_.size())
			current_round_ = 0;

		std::thread::id id = workers_[current_round_]->GetThreadId();
		++current_round_;

		return id;
	}
private:
	void ExpandPool(uint32_t size);
private:
	std::vector<SingleThreadUnit*> workers_;
	uint32_t current_round_ = 0 ;

};
#endif