//
// 缓存从系统分配的内存并供线程反复使用
//
// Created by 崔士杰 on 2019/8/8.

#ifndef SMP_POOL_H
#define SMP_POOL_H

#include <stack>
#include <pthread.h>

namespace smp {
	template<typename T>
	class Pool {
	public:
		Pool(): c(0) {
			pthread_mutex_init(&slock, NULL);
			pthread_mutex_init(&clock, NULL);
			s = new std::stack<T*>();
		}

		~Pool() {
			if (s != NULL) {
				clean();
				delete s;
				s = NULL;
			}

			pthread_mutex_destroy(&slock);
			pthread_mutex_destroy(&clock);
		}

		// 已分配的元素个数
		size_t cap() {
			pthread_mutex_lock(&clock);
			size_t n = c;
			pthread_mutex_unlock(&clock);

			return n;
		}

		// 取出
		T* get() {
			T* item = NULL;

			pthread_mutex_lock(&slock);
			if (!s->empty()) {
				item = s->top();
				s->pop();
				pthread_mutex_unlock(&slock);
				return item;
			}
			pthread_mutex_unlock(&slock);

			try {
				item = new T();
			} catch (const std::bad_alloc& e) {
				return NULL;
			}

			pthread_mutex_lock(&clock);
			c++;
			pthread_mutex_unlock(&clock);

			return item;
		}

		// 放入
		void put(T* item) {
			pthread_mutex_lock(&slock);
			s->push(item);
			pthread_mutex_unlock(&slock);
		}

		// 释放元素占用的内存
		void clean() {
			pthread_mutex_lock(&slock);
			while (!s->empty()) {
				delete s->top();
				s->pop();

				pthread_mutex_lock(&clock);
				c--;
				pthread_mutex_unlock(&clock);
			}
			pthread_mutex_unlock(&slock);
		}

	private:
		std::stack<T*>*	s;
		pthread_mutex_t slock;

		size_t c;
		pthread_mutex_t clock;

	private:
		Pool(Pool&);
		Pool& operator = (Pool&);
	};
}
#endif
