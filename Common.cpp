#include "stdafx.h"
#include "Common.h"
#include <Windows.h>
#include <time.h>
#include <direct.h>
#include <codecvt>
#include <fstream>

Common::Common()
{
}


Common::~Common()
{
}


//字符串转UTF8
string Common::GBK_2_UTF8(const std::string& gbkData)
{
	try
	{
		const char* GBK_LOCALE_NAME = "CHS";  //GBK在windows下的locale name(.936, CHS ), linux下的locale名可能是"zh_CN.GBK"

		std::wstring_convert<std::codecvt<wchar_t, char, mbstate_t>>
			conv(new std::codecvt<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
		std::wstring wString = conv.from_bytes(gbkData);    // string => wstring

		std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
		std::string utf8str = convert.to_bytes(wString);     // wstring => utf-8
		return utf8str;

	}
	catch (exception ex)
	{
		return gbkData;
	}
}

 
 
int Common::string_replase(string& s1, const string& s2, const string& s3)
{
	string::size_type pos = 0;
	string::size_type a = s2.size();
	string::size_type b = s3.size();
	while ((pos = s1.find(s2, pos)) != string::npos)
	{
		s1.replace(pos, a, s3);
		pos += b;
	}
	return 0;
}

//获取当前exe路径
string Common::GetExePath()
{

	char szFilePath[256] = { 0 };
	GetModuleFileNameA(NULL, szFilePath, 255);
	(strrchr(szFilePath, '\\'))[0] = 0; // 删除文件名，只获得路径字串  
	string path = szFilePath;
	return path;
}

//获取字体
string Common::GetFontFamily(int num)
{
	string str;
	switch (num)
	{
	case 0:
		str = "simhei.ttf";
		break;
	case 1:
		str = "simkai.ttf";
		break;
	case 2:
		str = "simsun.ttc";
		break;
	default:
		str = "simsun.ttc";
	}
	return str;
}


string Common::getNowTime()
{
	SYSTEMTIME st = { 0 };
	GetLocalTime(&st);  //获取当前时间 可精确到ms
	char myTime[30] = "\0";
	sprintf_s(myTime, "%d%02d%02d-%02d%02d%02d%03d",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		st.wMilliseconds);
	return myTime;
}

string Common::getNowTime2()
{
	time_t now_time = time(NULL);
	tm* t_tm = localtime(&now_time);
	char myTime[30] = "\0";  //strftime 第一个参数是 char * 
	strftime(myTime, sizeof(myTime), "%H%M%S", t_tm);  //asctime(t_tm)也可以
	return myTime;
}

/// <summary>
/// 分割字符串
/// </summary>
/// <param name="str"></param>
/// <param name="pattern"></param>
/// <returns></returns>
vector<string> Common::split(const string& str, const string& pattern)
{
	//const char* convert to char*
	char* strc = new char[strlen(str.c_str()) + 1];
	strcpy(strc, str.c_str());
	vector<string> resultVec;
	char* tmpStr = strtok(strc, pattern.c_str());
	while (tmpStr != NULL)
	{
		resultVec.push_back(string(tmpStr));
		tmpStr = strtok(NULL, pattern.c_str());
	}

	delete[] strc;

	return resultVec;
}


tm Common::stringToDatetime(string str)
{
	char* cha = (char*)str.data();             // 将string转换成char*。
	tm tm_;                                    // 定义tm结构体。
	int year, month, day, hour, minute, second;// 定义时间的各个int临时变量。
	sscanf(cha, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);// 将string存储的日期时间，转换为int临时变量。
	tm_.tm_year = year - 1900;                 // 年，由于tm结构体存储的是从1900年开始的时间，所以tm_year为int临时变量减去1900。
	tm_.tm_mon = month - 1;                    // 月，由于tm结构体的月份存储范围为0-11，所以tm_mon为int临时变量减去1。
	tm_.tm_mday = day;                         // 日。
	tm_.tm_hour = hour;                        // 时。
	tm_.tm_min = minute;                       // 分。
	tm_.tm_sec = second;                       // 秒。
	tm_.tm_isdst = 0;                          // 非夏令时。 
	return tm_;                                 // 返回值。
}