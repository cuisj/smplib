//
// 支持多线程的微型日志接口
//
// Created by 崔士杰 on 2019/8/8.

#ifndef SMP_ULOG_H
#define SMP_ULOG_H

#include <new>
#include <cstdio>
#include <cstdarg>
#include <pthread.h>
#include <string>

namespace smp {
	template<size_t N>
	class Ulog {
	public:
		static const int Lall	= 0;
		static const int Ltrace = 1;
		static const int Ldebug	= 2;
		static const int Linfo	= 3;
		static const int Lwarn	= 4;
		static const int Lerror	= 5;
		static const int Lfatal = 6;
		static const int Loff	= 7;

		static const int Fnone	= 0x00;
		static const int Fdate	= 0x01;
		static const int Ftime	= 0x02;
		static const int Fthrid	= 0x04;			// 线程标号
		static const int Fthrnm = 0x08;			// 线程名称, 需setThreadName
		static const int Flevel	= 0x10;
		static const int Fstd	= Fdate | Ftime;

		// 当所有使用者线程都退出时，调用此函数释放key
		// 如不释放也没关系
		static void clean() {
			pthread_once(&once_delete, key_delete);
		}

	public:
		Ulog(): level(Lall), flags(Fstd), logfd(1) {
			pthread_rwlock_init(&lock, NULL);
			pthread_once(&once_create, key_create);
		}

		~Ulog() {
			pthread_rwlock_destroy(&lock);
		}

		void setLogLevel(int logLevel) {
			pthread_rwlock_wrlock(&lock);
			level = (logLevel >= Lall && logLevel <= Loff) ? logLevel : Loff;
			pthread_rwlock_unlock(&lock);
		}

		void setOutput(int fd) {
			pthread_rwlock_wrlock(&lock);
			logfd = fd;
			pthread_rwlock_unlock(&lock);
		}

		void setFlags(int flag) {
			pthread_rwlock_wrlock(&lock);
			flags = flag;
			pthread_rwlock_unlock(&lock);
		}

		void setThreadName(const char* name) {
			Buffer* p = getBuffer();
			if (p == NULL)
				return;

			p->threadName = std::string(name);
		}

		// 较长日志, 整体发送
		void begin(int logLevel = Lall) {
			Buffer* p = getBuffer();
			if (p == NULL)
				return;

			p->longer = true;
			p->longer_level = (logLevel >= Lall && logLevel <= Loff) ? logLevel : Loff;

			pthread_rwlock_rdlock(&lock);
			if (p->longer_level < logLevel) {
				pthread_rwlock_unlock(&lock);
				return;
			}

			logprefix(p, levelNames[p->longer_level]);
			pthread_rwlock_unlock(&lock);
		}

		void print(const char *format, ...) {
			va_list args;
			va_start(args, format);
			loglonger(format, args);
			va_end(args);
		}

		void end() {
			Buffer* p = getBuffer();
			if (p == NULL)
				return;

			if (!p->longer)
				return;

			pthread_rwlock_rdlock(&lock);
			if (p->longer_level < level) {
				pthread_rwlock_unlock(&lock);

				p->len = 0;
				p->longer = false;
				return;
			}

			logwrite(logfd, p);
			pthread_rwlock_unlock(&lock);
			p->longer = false;
		}

		// 较短日志，立即发送
		void trace(const char* format, ...) {
			va_list args;
			va_start(args, format);
			logshorter(Ltrace, format, args);
			va_end(args);
		}

		void debug(const char* format, ...) {
			va_list args;
			va_start(args, format);
			logshorter(Ldebug, format, args);
			va_end(args);
		}

		void info(const char* format, ...) {
			va_list args;
			va_start(args, format);
			logshorter(Linfo, format, args);
			va_end(args);
		}

		void warn(const char* format, ...) {
			va_list args;
			va_start(args, format);
			logshorter(Lwarn, format, args);
			va_end(args);
		}

		void error(const char* format, ...) {
			va_list args;
			va_start(args, format);
			logshorter(Lerror, format, args);
			va_end(args);
		}

		void fatal(const char* format, ...) {
			va_list args;
			va_start(args, format);
			logshorter(Lfatal, format, args);
			va_end(args);
		}

	private:
		int level;
		int flags;
		int logfd;
		pthread_rwlock_t lock;

	private:
		Ulog(const Ulog&);
		Ulog& operator = (Ulog&);

	private:
		struct Buffer {
			Buffer(): len(0), longer(false) {
			}

			char	buf[N];
			int	len;

			bool	longer;
			int	longer_level;

			std::string threadName;
		};

		Buffer* getBuffer() {
			Buffer* p = (Buffer*)pthread_getspecific(key);
			if (p == NULL) {
				try {
					p = new Buffer();
				} catch (const std::bad_alloc& e) {
					return NULL;
				}

				pthread_setspecific(key, p);
			}

			return p;
		}

		void logprefix(Buffer* p, const char* lvname) {
			int n = 0;
			time_t timep;
			struct tm st_tm;

			if (flags & Fdate || flags & Ftime) {
				time(&timep);
				localtime_r(&timep, &st_tm);
			}

			if (flags & Fdate) {
				n = snprintf(p->buf + p->len, N - p->len, "%04d-%02d-%02d ", (1900 + st_tm.tm_year),
							(1 + st_tm.tm_mon), st_tm.tm_mday);
				if (n > 0 && n < N - p->len)
					p->len += n;
			}

			if (flags & Ftime) {
				n = snprintf(p->buf + p->len, N - p->len, "%02d:%02d:%02d ", st_tm.tm_hour,
							st_tm.tm_min, st_tm.tm_sec);
				if (n > 0 && n < N - p->len)
					p->len += n;
			}

			if (flags & Fthrid) {
				n = snprintf(p->buf + p->len, N - p->len, "[%ld] ", (long)pthread_self());
				if (n > 0 && n < N - p->len)
					p->len += n;
			}

			if (flags & Fthrnm && !p->threadName.empty()) {
				n = snprintf(p->buf + p->len, N - p->len, "[%s] ", p->threadName.c_str());
				if (n > 0 && n < N - p->len)
					p->len += n;
			}

			if (flags & Flevel && lvname != NULL) {
				n = snprintf(p->buf + p->len, N - p->len, "[%s] ", lvname);
				if (n > 0 && n < N - p->len)
					p->len += n;
			}
		}

		ssize_t logwrite(int logfd, Buffer* p) {
			ssize_t n = write(logfd, p->buf, p->len);
			p->len = 0;

			return n;
		}

		void loglonger(const char *format, va_list args) {
			int n;

			Buffer* p = getBuffer();
			if (p == NULL)
				return;

			if (!p->longer)
				return;

			pthread_rwlock_rdlock(&lock);
			if (p->longer_level < level) {
				pthread_rwlock_unlock(&lock);
				return;
			}

			n = vsnprintf(p->buf + p->len, N - p->len, format, args);
			if (n > 0 && n < N - p->len)
				p->len += n;

			pthread_rwlock_unlock(&lock);
		}

		void logshorter(int lv, const char* format, va_list args) {
			int n = 0;

			Buffer* p = getBuffer();
			if (p == NULL)
				return;

			if (p->longer) {
				// 我应该这么做吗？
				loglonger(format, args);
				return;
			}

			pthread_rwlock_rdlock(&lock);
			if (lv < level) {
				pthread_rwlock_unlock(&lock);
				return;
			}

			logprefix(p, levelNames[lv]);

			n = vsnprintf(p->buf + p->len, N - p->len, format, args);
			if (n > 0 && n < N - p->len)
				p->len += n;

			logwrite(logfd, p);
			pthread_rwlock_unlock(&lock);
		}

	private:
		static pthread_once_t once_create;
		static pthread_once_t once_delete;
		static pthread_key_t key;

		static void destructor(void* arg) {
			Buffer* p = (Buffer*)arg;
			delete p;
		}

		static void key_create() {
			pthread_key_create(&key, destructor);
		}

		static void key_delete() {
			pthread_key_delete(key);
		}

		static const char* const levelNames[];
	};

	template<size_t N>
	pthread_once_t Ulog<N>::once_create = PTHREAD_ONCE_INIT;

	template<size_t N>
	pthread_once_t Ulog<N>::once_delete = PTHREAD_ONCE_INIT;

	template<size_t N>
	pthread_key_t Ulog<N>::key;

	template<size_t N>
	const char* const Ulog<N>::levelNames[] = {NULL, "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", NULL};
}

#endif
