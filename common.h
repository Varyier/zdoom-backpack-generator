
#ifndef _BACKPACK_GEN_COMMON_H_
#define _BACKPACK_GEN_COMMON_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <exception>

namespace BpkGen {
	class Exception: public std::exception {
		const std::string m_message;
	public:
		explicit Exception(const std::string& message)
			: m_message(message)
		{}

		char const* what() const override {
			return m_message.c_str();
		}
	};

	typedef std::vector<std::string> StringArray;
	typedef std::unordered_map<std::string, std::string> StringToStringMap;

	typedef std::vector<char> CharArray;
}

#endif // _BACKPACK_GEN_COMMON_H_
