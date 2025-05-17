#pragma once
#define _PROTECTED protected
#define _PUBLIC public

#include<string>
#include<queue>
#include<mutex>
#include<memory>
#include<deque>

#include"Connection.h"
#include"User.h"

class AbstractUser;
class CommonUser;
class VipUser;

class BaseConnectionPool
{
_PUBLIC:
	BaseConnectionPool();
	BaseConnectionPool(const BaseConnectionPool&) = delete;
	BaseConnectionPool& operator=(const BaseConnectionPool&) = delete;

	string _ip;  // mysql的ip地址
	unsigned short _port;  // mysql的端口号 3306
	string _username;  // mysql登录用户名
	string _password;  // mysql登录密码
	string _dbname; // mysql数据库的名称
	int _initSize;  // 连接池的初始连接量
	int _maxSize;  // 连接池的最大连接量
	int _maxIdleTime;  // 连接池最大空闲时间
	//int _connectionTimeout;  // 连接池获取连接的超时时间

	atomic_int _connectionCnt;  // 记录连接所创建的connection连接的总数量

	deque<CommonUser*> commonUserDeque;  // 普通用户的排队列表
	deque<VipUser*> vipUserDeque;  // vip用户的排队列表

	atomic_bool _produceForVip;  // 判断是否在(initSize, maxSize)条件下发来的请求

	int _numberCount[3] = { 0,0,0 };

_PUBLIC:
	// 获取连接池对象实例
	//static BaseConnectionPool* getConnectionPool();

	// 从配置文件中加载配置项
	bool loadConfigFile();

	// 给外部提供接口从连接池中获取一个空闲连接
	virtual shared_ptr<Connection> getConnection(AbstractUser* _abUser);

	// 删除deque中已经决定退出排队的用户
	void deleteFromDeque(AbstractUser* _abUser);

	// 展示人数数据
	void show() const;

	// queue<Connection*> _connectQueue;  // 存储mysql连接的队列
	atomic_bool _priorUser;
	mutex _queueMutex;  // 维护连接队列的线程安全互斥锁
	atomic_int _designedForVip;  // 判断是否是为vip专门生产的连接
	condition_variable cv;  // 设置条件变量，用于连接生产线程和链接消费线程的通信
};