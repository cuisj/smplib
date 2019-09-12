//
// 在多个线程之间安全地传递数据
//
// Created by 崔士杰 on 2019/8/8.

#ifndef SMP_CHAN_H
#define SMP_CHAN_H

#include <queue>
#include <pthread.h>

namespace smp {
	template<typename T>
	class Chan {
	public:
		// 默认容量不限, 发送不会阻塞
		// 指定容量，容量空间满时发送会阻塞
		Chan(size_t capacity = 0): closed(false), c(capacity) {
			pthread_mutex_init(&qlock, NULL);
			pthread_cond_init(&qmore, NULL);
			pthread_cond_init(&qless, NULL);
		}

		~Chan() {
			pthread_mutex_destroy(&qlock);
			pthread_cond_destroy(&qmore);
			pthread_cond_destroy(&qless);
		}

		size_t len() {
			pthread_mutex_lock(&qlock);
			size_t len = q.size();
			pthread_mutex_unlock(&qlock);

			return len;
		}

		size_t cap() {
			return c;
		}

		// 注意: 发送(关闭)与接收操作必须处于不同线程

		// 向通道发送数据, 如果通道已关闭，立即返回false
		// 如果通道容量空间未满，则发送数据返回true, 否则, 等待通道有可用容量空间时发送，返回true
		bool operator << (const T& item) {
			pthread_mutex_lock(&qlock);
			if (closed) {
				pthread_mutex_unlock(&qlock);
				return false;
			}

			while(c > 0 && q.size() >= c) {
				pthread_cond_wait(&qless, &qlock);
			}

			q.push(item);
			pthread_mutex_unlock(&qlock);
			pthread_cond_signal(&qmore);
			return true;
		}

		// 关闭通道，不再向通道发送数据
		// close并不会清理掉未取出的元素
		void close() {
			pthread_mutex_lock(&qlock);
			closed = true;
			pthread_mutex_unlock(&qlock);
			pthread_cond_broadcast(&qmore);
		}

		// 从通道接收数据，有数据则返会返回true, 没有数据时会阻塞
		// 如果通道已被关闭，读完数据后立即返回false
		bool operator >> (T& item) {
			pthread_mutex_lock(&qlock);
			while (!closed && q.empty()) {
				pthread_cond_wait(&qmore, &qlock);
			}

			// 非空
			if (!q.empty()) {
				item = q.front();
				q.pop();
				pthread_mutex_unlock(&qlock);
				pthread_cond_signal(&qless);
				return true;
			}

			// 空了且已关闭
			pthread_mutex_unlock(&qlock);
			return false;
		}

	private:
		std::queue<T>	q;
		pthread_mutex_t qlock;
		pthread_cond_t	qmore;
		pthread_cond_t	qless;

		bool		closed;

		const size_t	c;

	private:
		Chan(Chan& ch);
		Chan& operator = (const Chan&);
	};
}
#endif
