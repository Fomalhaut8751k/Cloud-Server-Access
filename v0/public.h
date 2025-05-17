#pragma once

#define LOG(str) \
	cout << __FILE__ << ":" << __LINE__ << " " << \
	__TIMESTAMP__ << " : " << str << endl;
/*
	__FILE__：预定义宏，代表当前源文件名
	__LINE__：预定义宏，代表当前行号
	__TIMESTAMP__：预定义宏，代表源文件最后修改的日期和时间
*/

