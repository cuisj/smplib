//
// 支持多线程的ID资源管理
//
// Created by 崔士杰 on 2019/8/19.

#ifndef SMP_IDRS_H
#define SMP_IDRS_H

#include <ctime>
#include <pthread.h>
#include <sched.h>

namespace smp {
	template <size_t N>
	class Idrs {
	public:
		Idrs(time_t span): span(span), next(0) {
			for (int i = 0; i < N; i++) {
				ids[i].used = false;
				ids[i].timo = 0;
			}
			pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE);
		}

		~Idrs() {
			pthread_spin_destroy(&lock);
		}

		size_t get() {
			size_t id;
			size_t last = next;
			pthread_spin_lock(&lock);
			while (true) {
				// 已被占用且在有效期内
				if (ids[next].used && now() <= ids[next].timo) {
					if (++next >= N)
						next = 0;

					// 检查了一遍还没有，重新排队可执行队列
					if (next == last)
						sched_yield();

					continue;
				}

				id = next;
				ids[id].used = true;
				ids[id].timo = now() + span;

				if (++next >= N)
					next = 0;

				break;
			}
			pthread_spin_unlock(&lock);

			return id;
		}

		void put(size_t id) {
			pthread_spin_lock(&lock);
			if (id < N) {
				ids[id].used = false;
				ids[id].timo = 0;
			}
			pthread_spin_unlock(&lock);
		}

	private:
		Idrs(const Idrs&);
		Idrs& operator = (const Idrs&);

		time_t now() {
			return time(NULL);
		}

	private:
		struct ID {
			bool	used;
			time_t	timo;
		};

	private:
		const size_t span;

		pthread_spinlock_t lock;
		ID ids[N];
		size_t next;
	};
}

#endif
