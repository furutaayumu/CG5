#pragma once
#include <string> //wstring,string
class MiscUtility {
public:
	// string=>wstring　文字列変換
	std::wstring ConvertString(const std::string& str);

	// wstring=>string　文字列変換
	static std::string ConvertString(const std::wstring& str);
};
