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

	int _numberCount[3] = { 0,0,0 };

_PUBLIC:
	// ��ȡ���ӳض���ʵ��
	//static BaseConnectionPool* getConnectionPool();

	// �������ļ��м���������
	bool loadConfigFile();

	// ���ⲿ�ṩ�ӿڴ����ӳ��л�ȡһ����������
	virtual shared_ptr<Connection> getConnection(AbstractUser* _abUser);

	// ɾ��deque���Ѿ������˳��Ŷӵ��û�
	void deleteFromDeque(AbstractUser* _abUser);

	// չʾ��������
	void show() const;

	// queue<Connection*> _connectQueue;  // �洢mysql���ӵĶ���
	atomic_bool _priorUser;
	mutex _queueMutex;  // ά�����Ӷ��е��̰߳�ȫ������
	atomic_int _designedForVip;  // �ж��Ƿ���Ϊvipר������������
	condition_variable cv;  // ���������������������������̺߳����������̵߳�ͨ��
};