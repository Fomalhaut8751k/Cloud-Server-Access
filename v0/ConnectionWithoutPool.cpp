#include<iostream>
#include<string>
#include<thread>
#include<functional>
#include<atomic>
#include<condition_variable>

#include"ConnectionWithoutPool.h"
#include"Connection.h"
#include"User.h"

using namespace std;

ConnectionWithoutPool::ConnectionWithoutPool()
{
	_designedForVip = 0;  // 判断是否是为vip专门生产的连接
	_produceForVip = false;  // 判断是否在(initSize, maxSize)条件下发来的请求
	_priorUser = true;

	// 加载配置项
	if (!loadConfigFile())
	{
		return;
	}

	// 所有申请等到接收到申请时当场创建和连接

	// 启动一个新的线程，作为连接的生产者
	// thread produce(&ConnectionWithoutPool::produceConnectionTask, this);
	// produce.detach();

	// 启动一个新的线程，管理队列中空闲连接时间超过maxIdleTime的连接的释放,只保留initsize个空闲连接
	// thread timekeeper(&ConnectionWithoutPool::recycleConnectionTask, this);
	// timekeeper.detach();
}

// 线程安全的懒汉单例模式接口
//ConnectionWithoutPool* ConnectionWithoutPool::getConnectionWithoutPool()
//{
//	static ConnectionWithoutPool instance;
//	return &instance;
//}

// 从配置文件中加载配置项
bool ConnectionWithoutPool::loadConfigFile()
{
	FILE* pf = fopen("mysql.ini", "r");
	if (pf == nullptr)
	{
		LOG("mysql.ini file is not exist!");
		return false;
	}
	while (!feof(pf))  // 没有读到末尾(就继续读)
	{
		char line[1024] = {};
		fgets(line, 1024, pf); // 先读一行
		string str = line;
		int idx = str.find("=", 0);  // 例如ip=127.0.0.1，找key和value的分界点
		if (idx == -1)  // 注释或无效的配置项
		{
			continue;
		}
		int endidx = str.find("\n", idx);  // 从idx开始找结尾

		string key = str.substr(0, idx);  // 从0开始长度为idx就是key的字段
		string value = str.substr(idx + 1, endidx - idx - 1);  // 同上，value的字段

		//cout << key << ": " << value << endl;

		if (key == "ip")  // mysql的ip地址
		{
			_ip = value;
		}
		else if (key == "port")  // mysql的端口号 3306
		{
			_port = atoi(value.c_str());
		}
		else if (key == "username")  // mysql登录用户名
		{
			_username = value;
		}
		else if (key == "password")  // mysql登录密码
		{
			_password = value;
		}
		else if (key == "dbname")  // mysql数据库的名称
		{
			_dbname = value;
		}
		else if (key == "initSize")  // 连接池的初始连接量
		{
			_initSize = atoi(value.c_str());
		}
		else if (key == "maxSize")  // 连接池的最大连接量
		{
			_maxSize = atoi(value.c_str());
		}
		else if (key == "maxIdleTime")  // 连接池最大空闲时间
		{
			_maxIdleTime = atoi(value.c_str());
		}
	}
	return true;
}


// 给外部提供接口从连接池中获取一个空闲连接
shared_ptr<Connection> ConnectionWithoutPool::getConnection(AbstractUser* _abUser)
{
	unique_lock<mutex> lock(_queueMutex);

	_numberCount[0] += 1;  // 记录总人数

	cv.wait(lock,
		[&]() -> bool {
			return _priorUser == true;
		}
	);
	_priorUser = false;

	// 判断申请连接的是普通用户还是vip用户
	if (dynamic_cast<CommonUser*>(_abUser) != nullptr)
	{	// 如果是普通用户

		// 只判断一次，没有就排队
		// 如果总的申请人数不小于_initSize，那么普通用户就得排队
		if (_connectionApplyForcCommon + _connectionApplyForVip >= _initSize)
		{
			commonUserDeque.push_back(dynamic_cast<CommonUser*>(_abUser));
			cout << "【广播】";
			cout << "用户" << _abUser << ":\n——正在排队中......"
				<< "前面还有" << vipUserDeque.size() + commonUserDeque.size() << "人" << endl;

			for (VipUser* vip : vipUserDeque)
			{
				cout << vip << " ";
			}

			cout << " | ";

			for (CommonUser* common : commonUserDeque)
			{
				cout << common << " ";
			}

			cout << "\n" << endl;

			_priorUser = true;

			cv.notify_all();
			_abUser->_waiting = true;  // 表示用户处于等待状态

			cv.wait(lock, [&]() -> bool {
				// 有空闲连接 -> vip通道没有人在排队 -> 我是队头
				return (_abUser->_terminate == true) || (
					//(!_connectQueue.empty())
					(_connectionApplyForcCommon + _connectionApplyForVip < 10)
					&& vipUserDeque.empty()
					&& commonUserDeque.front() == _abUser
					&& (_designedForVip == 0)
					);
				}
			);
			// 因为离开而唤醒
			if (_abUser->_terminate == true)
			{
				// 从CommonUserDeque中删除
				_numberCount[1] += 1;  // 退出排队人数
				deleteFromDeque(_abUser);
				return nullptr;
			}
			// 因为有空闲连接可以申请了而唤醒
			else
			{
				// 退出排队
				commonUserDeque.pop_front();
			}
		}
	}
	else
	{   // 如果是vip用户

		// 如果已经超过了maxSize，即使是vip也得乖乖排队
		if (_connectionApplyForcCommon + _connectionApplyForVip >= _maxSize)
		{
			vipUserDeque.push_back(dynamic_cast<VipUser*>(_abUser));
			cout << "【广播】";
			cout << "用户" << _abUser << "(VIP):\n——正在排队中......为您开启vip通道，"
				<< "前面还有" << vipUserDeque.size() - 1 << "人" << endl;

			for (VipUser* vip : vipUserDeque)
			{
				cout << vip << " ";
			}

			cout << " | ";

			for (CommonUser* common : commonUserDeque)
			{
				cout << common << " ";
			}

			cout << "\n" << endl;

			_priorUser = true;

			cv.notify_all();
			_abUser->_waiting = true;  // 表示用户处于等待状态

			cv.wait(lock,
				[&]() -> bool {
					// 优先级：有空闲连接 -> 我是队头
					return (_abUser->_terminate == true) || (
						//(!_connectQueue.empty())
						(_connectionApplyForcCommon + _connectionApplyForVip < _maxSize)
						&& vipUserDeque.front() == _abUser
						);
				}
			);
			// 因为离开而唤醒
			if (_abUser->_terminate == true)
			{
				_numberCount[1] += 1;  // 退出排队人数
				// 从vipUserDeque中删除
				deleteFromDeque(_abUser);
				return nullptr;
			}
			// 因为有空闲连接可以申请了而唤醒
			else
			{
				// 退出排队
				vipUserDeque.pop_front();
			}
		}
		//// 如果没有超过，可以为vip用户申请专属的连接
		//else
		//{
		//	// 等待生产者生成新的连接
		//	_produceForVip = true;
		//	_designedForVip = 1;
		//	cv.notify_all();
		//	cv.wait(lock,
		//		[&]() -> bool {
		//			return // (!_connectQueue.empty())
		//				(_connectionApplyForcCommon + _connectionApplyForVip < _maxSize)
		//				&& (_designedForVip == 2);
		//		}
		//	);
		//	_designedForVip = 0;
		//}
		
	}

	Connection* p = new Connection();
	p->connect(_ip, _port, _username, _password, _dbname);
	// 刷新计时
	p->refreshAliveTime();

	shared_ptr<Connection> sp(p, 
		[&](Connection* pcon) {
			{
				unique_lock<mutex> lock(_queueMutex);
				// 刷新计时
				pcon->refreshAliveTime();

				/*_connectQueue.push(pcon);*/
				delete pcon;
			}
			// 有了空连接之后就应该....
			cv.notify_all();
		}
	);   // 取队头

	_numberCount[2] += 1;  // 申请到连接的人数

	if (dynamic_cast<CommonUser*>(_abUser) != nullptr)
	{
		cout << "【广播】";
		cout << "用户" << _abUser << ":\n——成功申请到了连接\n" << endl;
	}
	else
	{
		cout << "【广播】";
		cout << "用户" << _abUser << "(VIP):\n——成功申请到了连接\n" << endl;
	}

	_priorUser = true;
	//_connectQueue.pop();  // 然后弹出
	cv.notify_all();  // 消费后就通知

	return sp;
}

// 运行在独立的线程中，专门负责为vip用户生成新连接
//void ConnectionWithoutPool::produceConnectionTask()
//{
//	for (;;)
//	{
//		unique_lock<mutex> lock(_queueMutex);
//		// 队列不空，此处生产线程进入等待状态，等待指定条件满足并被唤醒
//
//		while (!_connectQueue.empty())
//		{
//			_designedForVip = 2;
//			cv.notify_all();
//			cv.wait(lock, [&]() -> bool {
//				// 只响应在可创建条件下的vip用户的创建请求
//				return (_produceForVip == true)
//					&& (_designedForVip == 1);
//				}
//			);
//		}
//		cout << "【广播】";
//		cout << "系统:\n——为vip用户创建新的连接....\n" << endl;
//		Connection* p = new Connection();
//		p->connect(_ip, _port, _username, _password, _dbname);
//		// 刷新计时
//		p->refreshAliveTime();
//
//		_connectQueue.push(p);
//		_connectionCnt++;
//
//		_produceForVip = false;
//
//		/*_designedForVip = 2;*/
//
//		cv.notify_all();  // 通知消费者线程，可以消费连接了
//	}
//}

// 运行在独立的线程中，管理队列中空闲连接时间超过maxIdleTime的连接的释放
//void ConnectionWithoutPool::recycleConnectionTask()
//{
//	/*
//		扫描超过maxIdleTime时间的空闲连接，进行多余的连接回收
//	*/
//	for (;;)
//	{
//		// 通过sleep模拟定时效果
//		this_thread::sleep_for(chrono::seconds(_maxIdleTime));
//		// 扫描整个队列，释放多余的连接
//		unique_lock<mutex> lock(_queueMutex);
//
//		while ((_connectionCnt > _initSize) && (!_connectQueue.empty()))  // 依然是从头开始删
//		{
//			Connection* p = _connectQueue.front();
//			if (p->getAliceTime() >= (_maxIdleTime * 1000))
//			{
//				_connectQueue.pop();
//				_connectionCnt--;
//				cout << "【广播】";
//				cout << "系统:\n——有连接超时将被释放，目前连接池还剩下"
//					<< _connectQueue.size() << "个\n" << endl;
//				delete p;  // 释放连接
//			}
//			else
//			{
//				break;  // 如果队头都没超时，那后面的也不会超时
//			}
//		}
//	}
//}

void ConnectionWithoutPool::deleteFromDeque(AbstractUser* _abUser)
{
	//unique_lock<std::mutex> lck(_queueMutex);

	if (dynamic_cast<CommonUser*>(_abUser) != nullptr)
	{
		CommonUser* _user = dynamic_cast<CommonUser*>(_abUser);
		deque<CommonUser*>::iterator it = std::find(commonUserDeque.begin(), commonUserDeque.end(), _user);
		if (it != commonUserDeque.end())
		{
			commonUserDeque.erase(it);
			cout << "【广播】";
			cout << "用户 " << _abUser << ":\n——退出了排队, 用户队列还有: " << commonUserDeque.size()
				<< "\n" << endl;
		}
		else
		{
			throw "Error: This user is not in the queue";
		}
	}
	else
	{
		VipUser* _user = dynamic_cast<VipUser*>(_abUser);
		deque<VipUser*>::iterator it = std::find(vipUserDeque.begin(), vipUserDeque.end(), _user);
		if (it != vipUserDeque.end())
		{
			vipUserDeque.erase(it);
			cout << "【广播】";
			cout << "用户" << _abUser << "(VIP):\n——退出了排队，VIP用户队列还有: " <<
				vipUserDeque.size() << endl;
		}
		else
		{
			cout << "pdcHelloWorld" << endl;
			throw "Error: This user is not in the queue";
		}
	}

	for (VipUser* vip : vipUserDeque)
	{
		cout << vip << " ";
	}

	cout << " | ";

	for (CommonUser* common : commonUserDeque)
	{
		cout << common << " ";
	}

	cout << "\n" << endl;

	cv.notify_all();

}

// 展示人数数据
void ConnectionWithoutPool::show() const
{
	cout << "申请连接的总人数: " << _numberCount[0] << "\n"
		<< "申请到连接的人数: " << _numberCount[2] << "\n"
		<< "中途退出排队的人数: " << _numberCount[1] << endl;
}