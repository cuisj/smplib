//
// 封装字符数组类型便于操作
//
// Created by 崔士杰 on 2019/8/8.

#ifndef SMP_BUFF_H
#define SMP_BUFF_H

#include <cstring>

namespace smp {
	template<typename T, size_t N>
	class Buff {
	public:
		T* data() {
			return b;
		}

		size_t size() {
			return N;
		}

	private:
		T b[N];
	};
}

#endif
