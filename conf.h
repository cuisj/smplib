//
// 解析配置文件
//
// Created by 崔士杰 on 2019/8/8.

//
// # 这是一个配置文件的示例
// gservername = "myproxy"
//
// [proxy]
//	serveraddr = 10.21.20.114	# 地址
//	serverport = 8080		# 端口
//	alloweduser = ["testuser1", "testuser2", "testuser5"]	# 允许的用户
//	allowedclient = ["10.21.20.115", "10.21.20.116"]	# 允许的客户端
//

#ifndef SMP_CONF_H
#define SMP_CONF_H

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <fstream>

namespace smp {
	template<size_t N>
	class Conf {
	public:
		class Value {
		public:
			Value(const char* v): val(v) {
			}

			Value(const Value& v): val(v.val) {
			}

			const std::string& toString() const {
				return val;
			}

			int toInt() const {
				return std::atoi(val.c_str());
			}

			long toLong() const {
				return std::atol(val.c_str());
			}

			double toFloat() const {
				return std::atof(val.c_str());
			}

			bool toBool() const {
				return (val != "0" && val != "false" && val != "False") ? true : false;
			}

		private:
			std::string val;
		};

		class Array {
		public:
			Array() {
				vect.reserve(16);
			}

			size_t size() const {
				return vect.size();
			}

			const Value* at(size_t i) const {
				return &vect[i];
			}

		private:
			friend class Conf;
			void add(const Value& v) {
				vect.push_back(v);
			}

		private:
			std::vector<Value> vect;
		};

	private:
		typedef std::map<std::string, Array*>	KV;	// {key: [value1, value2, ...]}


	public:
		Conf(const char* filename) {
			global = new KV();
			sections = new std::map<std::string, KV*>();
			cursec = global;
			loaded = false;

			if (load(filename) == 0)
				loaded = true;
		}

		bool loadOk() {
			return loaded;
		}

		~Conf() {
			for (typename std::map<std::string, Array*>::iterator it = global->begin(); it != global->end(); ++it) {
				delete it->second;
			}
			delete global;

			for (typename std::map<std::string, KV*>::iterator it = sections->begin(); it != sections->end(); ++it) {
				for (typename std::map<std::string, Array*>::iterator iter = it->second->begin();
						iter != it->second->end(); ++iter) {
					delete iter->second;
				}

				delete it->second;
			}

			delete sections;
		}

		void dump() {
			const Array* array = NULL;
			const Value* value = NULL;
			for (typename std::map<std::string, Array*>::iterator it = global->begin(); it != global->end(); ++it) {
				array = it->second;
				size_t len = array->size();

				std::cout << "{ \"" << it->first << "\" : [ ";
				for (size_t i = 0; i < len; i++) {
					value = array->at(i);

					std::cout << "\"" << value->toString() << "\"";
					if (i != len - 1)
						std::cout << ", ";
				}
				std::cout << " ] }" << std::endl;

			}

			for (typename std::map<std::string, KV*>::iterator it = sections->begin(); it != sections->end(); ++it) {
				std:: cout << "{ \"" << it->first << "\" : " << std::endl;
				for (typename std::map<std::string, Array*>::iterator iter = it->second->begin();
						iter != it->second->end(); ++iter) {
					array = iter->second;
					size_t len = array->size();

					std:: cout << "\t{ \"" << iter->first << "\" : [ ";
					for (size_t i = 0; i < len; i++) {
						value = array->at(i);

						std::cout << "\"" << value->toString() << "\"";
						if (i != len - 1)
							std::cout << ", ";
					}
					std::cout << " ] }" << std::endl;
				}
				std::cout << "}" << std::endl;
			}
		}

		const Array* getArray(const char* path) {
			const Array* array = NULL;

			if (!loaded)
				return NULL;

			array = findArray(path);
			if (array == NULL)
				return NULL;

			if (array->size() == 1)
				return NULL;

			return array;
		}

		const Value* getValue(const char* path) {
			const Array* array = NULL;

			if (!loaded)
				return NULL;

			array = findArray(path);
			if (array == NULL)
				return NULL;

			if (array->size() != 1)
				return NULL;

			return array->at(0);
		}

	private:
		Conf(const Conf&);
		Conf& operator = (const Conf&);

	private:
		const Array* findArray(const char* path) {
			const char *p = NULL;
			const char *q = NULL;
			const Array* array = NULL;

			if (path == NULL)
				return NULL;

			p = strchr(path, '.');
			if (p == NULL) {		// 从全局键值表查
				p = path;		// q: NULL, p: key
			} else {
				q = path;		// q: section, p: key
				p += 1;
			}

			if (q == NULL) {
				typename std::map<std::string, Array*>::iterator it = global->find(std::string(p));
				if (it != global->end())
					array = it->second;
			} else {
				typename std::map<std::string, KV*>::iterator it = sections->find(std::string(q, p - q - 1));
				if (it != sections->end()) {
					typename std::map<std::string, Array*>::iterator iter = it->second->find(std::string(p));
					if (iter != it->second->end())
						array = iter->second;
				}
			}

			return array;
		}

		int load(const char* filename) {
			int err = 0;

			char buff[N];
			std::ifstream ifs;

			ifs.open(filename, std::ifstream::in);
			if (ifs.fail())			// 打开文件失败
				err = -1;

			while (ifs.good()) {
				ifs.getline(buff, N);	// 结尾为'\0'
				if (!parse(buff)) {
					err = -2;
					break;
				}
			}

			ifs.close();

			return err;
		}

		// 格式错误返回false
		bool parse(char* buff) {
			char* b = NULL;
			char* p = NULL;
			char* k = NULL;
			char* v = NULL;
			char* t = NULL;

			Conf::Array* array = NULL;

			// 跳过空行
			if (*buff == '\0')
				return true;

			// 跳过空白字符
			//for (p = buff; *p != '\0' && isspace(*p); p++);
			//b = p;

			// 跳过注释行
			if (*buff == '#') {
				return true;
			}

			// 非空白字符开始的行可能是全局键值
			if (!isspace(*buff))
				cursec = global;

			// 跳过空白字符
			for (p = buff; *p != '\0' && isspace(*p); p++);
			b = p;

			// 去掉行尾的注释
			p = strchr(b, '#');
			if (p != NULL)
				*p = '\0';

			if (*b == '[') {		// 新的节
				p = strrchr(b, ']');
				if (p == NULL)
					return false;

				// 提取节名
				b += 1;
				*p = '\0';

				// 节名为空
				if (b == p)
					return true;

				t = trimSpace(b);
				if (*t == '\0')
					return true;

				// 创建节
				try {
					KV* kv = new KV();
					sections->insert(std::pair<std::string, KV*>(std::string(t), kv));
					cursec = kv;
				} catch (std::bad_alloc& e) {
					return false;
				}

			} else { // 键值对
				p = strchr(b, '=');
				if (p == NULL)
					return true;

				// 分离键值
				*p = '\0';

				k = trimSpace(b);
				v = trimSpace(p + 1);

				// 键为空
				if (*k == '\0')
					return true;

				// 准备存值
				if (*v == '[') { // 值为列表
					p = strrchr(v, ']');
					if (p == NULL)
						return false;

					// 提取列表
					v = v + 1;
					*p = '\0';

					// 列表为空
					if (v == p)
						return true;

					t = trimSpace(v);
					if (*t == '\0')
						return true;

					try {
						array = new Array();
					} catch (std::bad_alloc& e) {
						return false;
					}

					// 提取列表值
					b = v;
					do {
						p = strchr(b, ',');
						if (p == NULL) {
							t = trimSpace(b);
							if (*t != '\0')
								array->add(Value(t));

						} else {
							*p = '\0';
							t = trimSpace(b);
							if (*t != '\0')
								array->add(Value(t));

							b = p + 1;
						}
					} while (p != NULL);
				} else {
					// 值为空
					if (*v == '\0')
						return true;

					try {
						array = new Array();
					} catch (std::bad_alloc& e) {
						return false;
					}

					t = trimSpace(b);
					if (*t != '\0')
						array->add(Value(v));
				}

				if (array != NULL) {
					if (array->size() == 0) {
						delete array;
						return true;
					}

					cursec->insert(std::pair<std::string, Array*>(std::string(k), array));
				}
			}

			return true;
		}

		char* trimSpace(char* s) {
			char* p = NULL;
			char* q = NULL;
			size_t len = strlen(s);

			if (len <= 0)
				return s;

			for (p = s; *p != '\0' && (isspace(*p) || *p == '"' || *p == '\''); p++);

			for (q = s + len - 1; q != p; q--) {
				if (!isspace(*q) && *q != '"' && *q != '\'') {
					*(q + 1) = '\0';
					break;
				}
			}

			return p;
		}

	private:
		KV* global;
		std::map<std::string, KV*>* sections;
		KV* cursec;

		bool loaded;
	};
}

#endif
