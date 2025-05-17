#pragma once
/*
	实现MySQL数据库的操作
*/
#include<iostream>
#include <mysql.h>
#include <string>

using namespace std;
#include "public.h"


class Connection
{
public:
	// 初始化数据库连接
	Connection();

	// 释放数据库连接资源
	~Connection();

	// 连接数据库
	bool connect(string ip, unsigned short port, string user, string password,
		string dbname);

	// 更新操作 insert、delete、update
	bool update(string sql);

	// 查询操作 select
	MYSQL_RES* query(string sql);

	// 刷新连接的起始的空闲时间点
	void refreshAliveTime();
	/*
		包含入队操作的有：独立线程的生产者的生产的线程；用户归还(析构)的线程
	*/

	// 返回存活的时间
	clock_t getAliceTime() const;

private:
	MYSQL* _conn; // 表示和MySQL Server的一条连接
	clock_t _alivetime;  // 记录进入空闲状态后的存活时间
};