#include<iostream>
#include<string>
#include<thread>
#include<functional>
#include<mutex>

#include"Connection.h"
#include"ConnectionPool.h"
#include"User.h"

using namespace std;

bool __PRINT_ = false;

AbstractUser::AbstractUser(int exit) :
	_Connection(nullptr),
	_alivetime(clock()),
	_timeOut(10),
	_waiting(false),
	_terminate(false),
	_exitOrNot(exit)
{

}

// 用户发起连接请求
void AbstractUser::toConnect(ConnectionPool* _pConnectPool)
{
	//_Connection = _pConnectPool->getConnection(this);
	thread t0(
		[&]() -> void {
			_Connection = _pConnectPool->getConnection(this);
		}
	);
	if (_Connection == nullptr)
	{
		thread t1(
			[&]() -> void {
				if (_Connection == nullptr)
				{
					// 每5秒判断一次行为
					std::this_thread::sleep_for(std::chrono::milliseconds(5000));
					if (_Connection == nullptr && _waiting)
					{
						if (_exitOrNot == 1)
						{
							unique_lock<std::mutex> lck(_pConnectPool->_queueMutex);

							this->_terminate = true;
							_Connection.reset();
							_Connection == nullptr;

							_pConnectPool->cv.notify_all();
						}
					}
				}
			}
		);
		// 如果申请到连接了还没有退出排队
		t1.join();
		t0.join();
	}

	if (_Connection != nullptr)
	{
		update();
		thread t2(bind(&AbstractUser::timeoutRecycleConnect, this, _pConnectPool));
		t2.join();
	}

}

// 用户发起连接请求（不使用连接池）
void AbstractUser::toConnectWithoutConnectionPool()
{

}

// 细分用户行为一
void AbstractUser::update()
{
	// 触发一次事件，更新一下时间
	refreshAliveTime();
	if (_Connection != nullptr)
	{
		char sql[1024] = { 0 };
		sprintf(sql, "insert into user(name, age, sex) values('%s', '%d', '%s')",
			"zhang san", 20, "male");
		_Connection->update(sql);
	}
}

// 刷新连接的起始的空闲时间点
void AbstractUser::refreshAliveTime()
{
	_alivetime = clock();
}

// 返回存活的时间
clock_t AbstractUser::getAliceTime() const
{
	return clock() - _alivetime;
}

// 长时间未操作的超时回收
void CommonUser::timeoutRecycleConnect(ConnectionPool* _connectionPool)
{
	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (getAliceTime() >= 10 * 1000)
		{
			{
				unique_lock<mutex> lck(_connectionPool->_queueMutex);
				// 调用其删除器，归还连接
				if (__PRINT_)
				{
					cout << "【广播】";
					cout << "用户" << this << "\n——长时间没有操作，已回收连接\n" << endl;
					//cout << "【广播】";
					//cout << "系统" << ":\n——有连接被回收，目前连接池中还剩: "
					//	<< _connectionPool->_connectQueue.size() + 1
					//	<< "个空闲连接\n" << endl;
				}
			}
			_Connection.reset();
			_Connection = nullptr;
			_connectionPool->_designedForVip = 0;
			break;
		}
	}
}

void VipUser::timeoutRecycleConnect(ConnectionPool* _connectionPool)
{
	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (getAliceTime() >= 10 * 1000)
		{
			{
				unique_lock<mutex> lck(_connectionPool->_queueMutex);
				// 调用其删除器，归还连接
				if (__PRINT_)
				{
					cout << "【广播】";
					cout << "用户" << this << "(VIP):\n——长时间没有操作，已回收连接\n" << endl;
					cout << "【广播】";
					//cout << "系统" << ":\n——有连接被回收，目前连接池中还剩: "
					//	<< _connectionPool->_connectQueue.size() + 1
					//	<< "个空闲连接\n" << endl;
				}
			}
			_Connection.reset();
			_Connection = nullptr;
			_connectionPool->_designedForVip = 0;
			break;
		}
	}
}

void AbstractUser::userBehavior()
{

}

CommonUser::CommonUser(int exit) :
	AbstractUser(exit)
{

}

VipUser::VipUser(int exit) :
	AbstractUser(exit)
{

}