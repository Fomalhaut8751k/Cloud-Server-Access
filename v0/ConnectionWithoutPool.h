#pragma once
/*
	��ʹ�����ӳأ�����ѹ�����ԶԱ�
*/
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


class ConnectionWithoutPool
{
public:
	ConnectionWithoutPool();
	// ��ȡ���ӳض���ʵ��
	//static ConnectionWithoutPool* getConnectionWithoutPool();

	// �������ļ��м���������
	bool loadConfigFile();

	// ���ⲿ�ṩ�ӿڴ����ӳ��л�ȡһ����������
	shared_ptr<Connection> getConnection(AbstractUser* _abUser);

	// ɾ��deque���Ѿ������˳��Ŷӵ��û�
	void deleteFromDeque(AbstractUser* _abUser);

	// չʾ��������
	void show() const;

	// queue<Connection*> _connectQueue;  // �洢mysql���ӵĶ���
	atomic_bool _priorUser;
	mutex _queueMutex;  // ά�����Ӷ��е��̰߳�ȫ������
	atomic_int _designedForVip;  // �ж��Ƿ���Ϊvipר������������
	condition_variable cv;  // ���������������������������̺߳����������̵߳�ͨ��

private:
	
	ConnectionWithoutPool(const ConnectionWithoutPool&) = delete;
	ConnectionWithoutPool& operator=(const ConnectionWithoutPool&) = delete;

	//// �����ڶ������߳��У�ר�Ÿ�������������
	//void produceConnectionTask();

	//// �����ڶ������߳��У���������п�������ʱ�䳬��maxIdleTime�����ӵ��ͷ�
	//void recycleConnectionTask();

	string _ip;  // mysql��ip��ַ
	unsigned short _port;  // mysql�Ķ˿ں� 3306
	string _username;  // mysql��¼�û���
	string _password;  // mysql��¼����
	string _dbname; // mysql���ݿ������
	int _initSize;  // ���ӳصĳ�ʼ������
	int _maxSize;  // ���ӳص����������
	int _maxIdleTime;  // ���ӳ�������ʱ��
	//int _connectionTimeout;  // ���ӳػ�ȡ���ӵĳ�ʱʱ��

	atomic_int _connectionCnt;  // ��¼������������connection���ӵ�������

	deque<CommonUser*> commonUserDeque;  // ��ͨ�û����Ŷ��б�
	deque<VipUser*> vipUserDeque;  // vip�û����Ŷ��б�

	atomic_bool _produceForVip;  // �ж��Ƿ���(initSize, maxSize)�����·���������

	atomic_int _connectionApplyForcCommon;  // ��¼�Ѿ�����ͨ�û������ȥ������
	atomic_int _connectionApplyForVip;  // ��¼�Ѿ���vip�û������ȥ������

	int _numberCount[3] = { 0,0,0 };
};