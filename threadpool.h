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


struct Task
{
	int ownner_;	//识别这个
	std::function<void()> task_;
};

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

	bool PopTask(Task& task)
	{
		std::unique_lock<std::mutex> lock(this->queue_mutex);
		if (tasks.empty())
			return false;
		task = tasks.front();
		tasks.pop();
		return true;
	}

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
		static int round_count = 1;	
		workers_[current_round_]->enqueue(func);
		if (current_round_ == workers_.size())
		{
			++round_count;	//轮完一圈了
			current_round_ = 0;
		}

		std::thread::id id = workers_[current_round_]->GetThreadId();
		++current_round_;

		workers_[current_round_]->enqueue(func);

		if (round_count % 300 == 0)	//300这个数字是随便定的，具体怎么确定不知道，后面看情况改
		{
			BalanceThreadLoad();
		}
		return id;
	}

	void BalanceThreadLoad()	//worker steal 算法,从其他线程队列中偷出任务塞到负载较少的队列
	{
		int total_task = 0;
		std::vector<int> worker_task(workers_.size());
		for (int i = 0; i < workers_.size(); i++)
		{
			worker_task[i] = workers_[i]->GetTaskSize();
			total_task += worker_task[i];
		}

		if (workers_.size() == 0)
			return;
		int average = total_task / workers_.size();
		if (average == 0)
			return;

		for (int i = 0; i < worker_task.size(); i++)
		{
			worker_task[i] -= average;	//负载的情况，正值表示较忙，负值表示较空闲
		}
		auto GetIndex = [&worker_task](std::function<bool(int)> f)->int {
			for (auto index : worker_task)
			{
				if (f(worker_task[index]))
					return index;
			}
			return -1;
		};
		int first_neg_id, first_pos_id;
		while (true)
		{
			first_neg_id = GetIndex([](int val)->bool {	//空闲线程
				return val < 0;
			});
			if (first_neg_id < 0)
				break;

			first_pos_id = GetIndex([](int val)->bool {	//选第一个繁忙线程
				return val > 0;
			});

			int diff_top_botton = worker_task[first_neg_id] + worker_task[first_pos_id];
			int migrate_task_num = 0;
			if (diff_top_botton > 0)	//task_bussy - task_free > average
			{
				migrate_task_num = -worker_task[first_neg_id];	//迁移的数量=此空闲线程 -平均任务数量
				worker_task[first_neg_id] = 0;
				worker_task[first_pos_id] = diff_top_botton;
			}
			else //空闲线程太闲
			{
				migrate_task_num = worker_task[first_pos_id];
				worker_task[first_neg_id] = diff_top_botton;
				worker_task[first_pos_id] = 0;
			}

			SingleThreadUnit *from = workers_[first_pos_id];
			SingleThreadUnit *to = workers_[first_neg_id];

			for (int i = 0; i < migrate_task_num; i++)
			{
				Task task;
				bool ok = from->PopTask(task);
				if(!ok)
					continue;
				to->enqueue(task.task_);
			}
		}
		SingleThreadUnit *from = workers_[first_pos_id];
		for (int i = 1; i < worker_task[first_pos_id]; i++)
		{
			SingleThreadUnit*to = workers_[i - 1];
			if (from->GetThreadId() == to->GetThreadId())
				continue;
			Task task;
			bool ok = from->PopTask(task);
			to->enqueue(task.task_);
		}

	}
private:
	void ExpandPool(uint32_t size);
private:
	std::vector<SingleThreadUnit*> workers_;
	uint32_t current_round_ = 0 ;
	bool stop_;

};
#endif