#include<iostream>
#include<vector>

#include"public.h"
#include"Connection.h"
#include"ConnectionPool.h"
#include"User.h"

using namespace std;

int main()
{
    srand((unsigned)time(NULL));
    clock_t begin = clock();

    ConnectionPool* _connectPool = ConnectionPool::getConnectionPool();
    vector<shared_ptr<CommonUser>> _vecCommonUser;
    vector<shared_ptr<VipUser>> _vecVipUser;

    vector<thread> _vecThread;

    // ###### 单独测试vip用户 ##################################################################
#if 0
    vector<int> _vecUserType;
    int arr[60] = {};

    for (int i = 0; i < 60; i++)
    {
        arr[i] = rand() % 2;
    }
    for (int i = 0; i < 60; ++i)
    {
        //_vecCommonUser.push_back(shared_ptr<CommonUser>(new CommonUser));
        _vecVipUser.push_back(shared_ptr<VipUser>(new VipUser(arr[i])));
    }

    for (shared_ptr<VipUser>& _pUser : _vecVipUser)
    {
        _vecThread.push_back(std::thread([&]()->void {
            _pUser->toConnect(_connectPool);
            }
        ));
    }

    for (thread& t : _vecThread)
    {
        t.join();
    }

#endif
    // ###### 单独测试普通用户 #################################################################
#if 0
    int arr[60] = {};
    for (int i = 0; i < 60; i++)
    {
        arr[i] = rand() % 2;
    }
    for (int i = 0; i < 60; ++i)
    {
        _vecCommonUser.push_back(shared_ptr<CommonUser>(new CommonUser(arr[i])));
    }

    for (shared_ptr<CommonUser>& _pUser : _vecCommonUser)
    {
        _vecThread.push_back(std::thread([&]()->void {
            _pUser->toConnect(_connectPool);
            }
        ));
    }

    for (thread& t : _vecThread)
    {
        t.join();
    }

#endif
    // ###### 普通用户和vip用户 ###############################################################
#if 1
    vector<int> _vecUserType;
    int arr[500] = {};
    for (int i = 0; i < 500; i++)
    {
        arr[i] = rand() % 2;
    }

    for (int i = 0; i < 500; ++i)
    {
        // 0表示普通用户，1表示vip用户
        int _userType = rand() % 2;
        _vecUserType.push_back(_userType);
        if (_userType == 1)
        {
            _vecVipUser.push_back(shared_ptr<VipUser>(new VipUser(arr[i])));
        }
        else
        {
            _vecCommonUser.push_back(shared_ptr<CommonUser>(new CommonUser(arr[i])));
        }
    }

    auto _itVecVipUser = _vecVipUser.begin();
    auto _itVecCommonUser = _vecCommonUser.begin();

    for (int& userType : _vecUserType)
    {
        if (userType == 1)
        {
            _vecThread.push_back(std::thread(
                [&]() -> void {
                    (*_itVecVipUser++)->toConnect(_connectPool);
                }
            )
            );
        }
        else
        {
            _vecThread.push_back(std::thread(
                [&]() -> void {
                    (*_itVecCommonUser++)->toConnect(_connectPool);
                }
            )
            );
        }
    }

    for (thread& t : _vecThread)
    {
        t.join();
    }

#endif
    // ###### 压力测试 ###########################################
#if 1

    clock_t end = clock();

    cout << (end - begin) << "ms" << endl;


#endif

    _connectPool->show();

    return 0;
}
