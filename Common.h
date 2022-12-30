#pragma once
#include <string>
#include <vector>

using namespace std;
class Common
{
public:
	Common();
	~Common();
	static string Common::GBK_2_UTF8(const string& gbkStr);
	static int string_replase(string& s1, const string& s2, const string& s3);
	static	string GetExePath();
	static	string GetFontFamily(int num);
	static  string getNowTime();
	static	string getNowTime2();
	static vector<string> split(const string& str, const string& pattern);
	static tm stringToDatetime(string str);

};

