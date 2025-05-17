#include<iostream>
#include<string>
#include<thread>
#include<functional>
#include<atomic>
#include<condition_variable>

#include"BaseConnectionPool.h"
#include"Connection.h"
#include"User.h"

using namespace std;

BaseConnectionPool::BaseConnectionPool()
{
	_designedForVip = 0;  // 判断是否是为vip专门生产的连接
	_produceForVip = false;  // 判断是否在(initSize, maxSize)条件下发来的请求
	_priorUser = true;

	// 加载配置项
	if (!loadConfigFile())
	{
		return;
	}
}

//// 线程安全的懒汉单例模式接口
//BaseConnectionPool* BaseConnectionPool::getConnectionPool()
//{
//	static BaseConnectionPool instance;
//	return &instance;
//}

// 从配置文件中加载配置项
bool BaseConnectionPool::loadConfigFile()
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

shared_ptr<Connection> BaseConnectionPool::getConnection(AbstractUser* _abUser)
{
	// 空实现
}

void BaseConnectionPool::deleteFromDeque(AbstractUser* _abUser)
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
void BaseConnectionPool::show() const
{
	cout << "申请连接的总人数: " << _numberCount[0] << "\n"
		<< "申请到连接的人数: " << _numberCount[2] << "\n"
		<< "中途退出排队的人数: " << _numberCount[1] << endl;
}