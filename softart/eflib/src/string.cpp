#include <eflib/include/string/string.h>

#include <cstdlib>
#include <vector>

using namespace std;

vector<char> _string_buf;

namespace eflib{
	string to_ansi_string(const wstring& instr){
		//����ռ�
#ifdef EFLIB_MSVC
		size_t required;
		wcstombs_s(&required, NULL, 0, instr.c_str(), 0);
#else
		size_t required = std::wcstombs(NULL, instr.c_str(), 0);
#endif
		_string_buf.resize(required + 1);

		//ת��
#ifdef EFLIB_MSVC
		size_t l;
		wcstombs_s(&l, &(_string_buf[0]), _string_buf.size() * sizeof(_string_buf[0]), instr.c_str(), required + 1);
#else
		size_t l = std::wcstombs(&(_string_buf[0]), instr.c_str(), required+1);
#endif
		if(l == size_t(-1)){
			return string();
		}

		return string((char*)(&_string_buf[0]));
	}

	string to_ansi_string(const string& instr){
		return instr;
	}

	bool to_ansi_string(string& outstr, const wstring& instr){
		//����ռ�
#ifdef EFLIB_MSVC
		size_t required;
		wcstombs_s(&required, NULL, 0, instr.c_str(), 0);
#else
		size_t required = std::wcstombs(NULL, instr.c_str(), 0);
#endif
		outstr.resize(required + 1);

		//ת��
#ifdef EFLIB_MSVC
		size_t l;
		wcstombs_s(&l, &(outstr[0]), outstr.size() * sizeof(outstr[0]), instr.c_str(), required + 1);
#else
		size_t l = std::wcstombs(&(outstr[0]), instr.c_str(), required+1);
#endif
		if(l == size_t(-1)){
			return false;
		}

		outstr.resize(l);
		return true;
	}

	bool to_ansi_string(string& outstr, const string& instr){
		outstr = instr;
		return true;
	}

	wstring to_wide_string(const wstring& instr){
		return instr;
	}

	wstring to_wide_string(const string& instr){
		//����ռ�
#ifdef EFLIB_MSVC
		size_t required;
		mbstowcs_s(&required, NULL, 0, instr.c_str(), 0);
#else
		size_t required = std::mbstowcs(NULL, instr.c_str(), 0);
#endif
		_string_buf.resize((required + 1) * 2);

		//ת��
#ifdef EFLIB_MSVC
		size_t l;
		mbstowcs_s(&l, (wchar_t*)&(_string_buf[0]), _string_buf.size() * sizeof(_string_buf[0]), instr.c_str(), required + 1);
#else
		size_t l = std::mbstowcs((wchar_t*)&(_string_buf[0]), instr.c_str(), required+1);
#endif
		if(l == size_t(-1)){
			return wstring();
		}

		return wstring((wchar_t*)(&_string_buf[0]));
	}

	bool to_wide_string(wstring& outstr, const string& instr)
	{
		//����ռ�
#ifdef EFLIB_MSVC
		size_t required;
		mbstowcs_s(&required, NULL, 0, instr.c_str(), 0);
#else
		size_t required = mbstowcs(NULL, instr.c_str(), 0);
#endif
		outstr.resize(required + 1);

		//ת��
#ifdef EFLIB_MSVC
		size_t l;
		mbstowcs_s(&l, &(outstr[0]), outstr.size() * sizeof(outstr[0]), instr.c_str(), required + 1);
#else
		size_t l = mbstowcs(&(outstr[0]), instr.c_str(), required+1);
#endif
		if(l == size_t(-1)){
			return false;
		}

		outstr.resize(l);
		return true;
	}

	bool to_wide_string(wstring& outstr, const wstring& instr){
		outstr = instr;
		return true;
	}
}
