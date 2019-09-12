//
// 支持多线程的单例模板
//
// Created by 崔士杰 on 2019/8/8.

#ifndef SMP_SINGLE_H
#define SMP_SINGLE_H

#include <pthread.h>

namespace smp {
	template <typename T>
	class Sole {
	public:
		static T* instance() {
			pthread_once(&once_create, init);
			return item;
		}

		static void destroy() {
			pthread_once(&once_delete, free);
		}

	private:
		Sole();
		Sole(const Sole&);
		Sole& operator = (const Sole&);

		static void init() {
			item = new T();
		}

		static void free() {
			if (item != NULL) {
				delete item;
				item = NULL;
			}
		}

	private:
		static T* item;
		static pthread_once_t once_create;
		static pthread_once_t once_delete;
	};

	template <typename T>
	T* Sole<T>::item = NULL;

	template <typename T>
	pthread_once_t Sole<T>::once_create = PTHREAD_ONCE_INIT;

	template <typename T>
	pthread_once_t Sole<T>::once_delete = PTHREAD_ONCE_INIT;
}

#endif
