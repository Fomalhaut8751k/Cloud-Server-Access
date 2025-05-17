#pragma once
#include<iostream>
#include<mutex>
#include<condition_variable>

#include"ConnectionPool.h"
#include"Connection.h"

class ConnectionPool;

class AbstractUser
{
public:
	AbstractUser(int exit = 0);

	// 用户发起连接请求
	void toConnect(ConnectionPool* _pConnectPool);

	// 用户发起连接请求（不使用连接池）
	void toConnectWithoutConnectionPool();

	// 用户行为一
	void update();

	// 刷新连接的起始的空闲时间点
	void refreshAliveTime();

	// 返回存活的时间
	clock_t getAliceTime() const;

	// 长时间未操作的超时回收
	virtual void timeoutRecycleConnect(ConnectionPool* _connectionPool) = 0;

	// 用户行为
	void userBehavior();

	// 用户等待标记
	atomic_bool _waiting;

	// 线程终止标记
	atomic_bool _terminate;

	int _exitOrNot;

protected:
	shared_ptr<Connection> _Connection;
	clock_t _alivetime;  // 记录进入用户不操作时间
	int _timeOut;
	std::mutex _userMtx;
};

// 普通用户
class CommonUser : public AbstractUser
{
public:
	CommonUser(int exit = 0);

	void timeoutRecycleConnect(ConnectionPool* _connectionPool);
};


// VIP用户
class VipUser : public AbstractUser
{
public:
	VipUser(int exit = 0);

	void timeoutRecycleConnect(ConnectionPool* _connectionPool);
};