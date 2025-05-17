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
	_designedForVip = 0;  // �ж��Ƿ���Ϊvipר������������
	_produceForVip = false;  // �ж��Ƿ���(initSize, maxSize)�����·���������
	_priorUser = true;

	// ����������
	if (!loadConfigFile())
	{
		return;
	}
}

//// �̰߳�ȫ����������ģʽ�ӿ�
//BaseConnectionPool* BaseConnectionPool::getConnectionPool()
//{
//	static BaseConnectionPool instance;
//	return &instance;
//}

// �������ļ��м���������
bool BaseConnectionPool::loadConfigFile()
{
	FILE* pf = fopen("mysql.ini", "r");
	if (pf == nullptr)
	{
		LOG("mysql.ini file is not exist!");
		return false;
	}
	while (!feof(pf))  // û�ж���ĩβ(�ͼ�����)
	{
		char line[1024] = {};
		fgets(line, 1024, pf); // �ȶ�һ��
		string str = line;
		int idx = str.find("=", 0);  // ����ip=127.0.0.1����key��value�ķֽ��
		if (idx == -1)  // ע�ͻ���Ч��������
		{
			continue;
		}
		int endidx = str.find("\n", idx);  // ��idx��ʼ�ҽ�β

		string key = str.substr(0, idx);  // ��0��ʼ����Ϊidx����key���ֶ�
		string value = str.substr(idx + 1, endidx - idx - 1);  // ͬ�ϣ�value���ֶ�

		//cout << key << ": " << value << endl;

		if (key == "ip")  // mysql��ip��ַ
		{
			_ip = value;
		}
		else if (key == "port")  // mysql�Ķ˿ں� 3306
		{
			_port = atoi(value.c_str());
		}
		else if (key == "username")  // mysql��¼�û���
		{
			_username = value;
		}
		else if (key == "password")  // mysql��¼����
		{
			_password = value;
		}
		else if (key == "dbname")  // mysql���ݿ������
		{
			_dbname = value;
		}
		else if (key == "initSize")  // ���ӳصĳ�ʼ������
		{
			_initSize = atoi(value.c_str());
		}
		else if (key == "maxSize")  // ���ӳص����������
		{
			_maxSize = atoi(value.c_str());
		}
		else if (key == "maxIdleTime")  // ���ӳ�������ʱ��
		{
			_maxIdleTime = atoi(value.c_str());
		}
	}
	return true;
}

shared_ptr<Connection> BaseConnectionPool::getConnection(AbstractUser* _abUser)
{
	// ��ʵ��
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
			cout << "���㲥��";
			cout << "�û� " << _abUser << ":\n�����˳����Ŷ�, �û����л���: " << commonUserDeque.size()
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
			cout << "���㲥��";
			cout << "�û�" << _abUser << "(VIP):\n�����˳����Ŷӣ�VIP�û����л���: " <<
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

// չʾ��������
void BaseConnectionPool::show() const
{
	cout << "�������ӵ�������: " << _numberCount[0] << "\n"
		<< "���뵽���ӵ�����: " << _numberCount[2] << "\n"
		<< "��;�˳��Ŷӵ�����: " << _numberCount[1] << endl;
}