## 主要功能点和问题分析
- ### 不同阶段的连接申请和排队

    > **阶段一：**
    

    连接池_connectQueue中的连接加上被申请走的连接数量等于initSize, 如果此时_connectQueue中还有空闲连接，那么无论是普通用户还是vip用户都可以直接申请里面的连接。如果_connectQueue中的连接空了，就进入第二阶段。

    > **阶段二：**

    此时总的连接数小于maxSize，普通用户想再申请连接，就要**排队**，而vip用户想要再申请连接，可以让生产者生产额外的连接，但是总的数量不能超过maxSize。当总的数量大于等于maxSize时，就进入第三阶段。

    > **阶段三：**

    此时总的连接数大于等于maxSize，即使是vip用户，也得进行排队。

     **排队机制：**

    在ConnectionPool中设置两个双端队列deque: 
    ```cpp
    deque<CommonUser*> commonUserDeque;
    deque<VipUser*> vipUserDeque;
    ```
    双端队列支持：

    push_back()——模拟入队<br>
    pop_front()——模拟出队<br>
    erase(iterator it)——通过遍历确定迭代器然后删除，模拟用户退出排队<br>

    ### 用户进入排队等待状态

    比起之前连接池的项目，不再有连接池获取连接的超时时间_connectionTimeout。

    - 对于普通用户，如果判断_connectQueue为空，则将该用户对应的指针放入commonUserDeque中兵进入等待状态，接受cv.notify_all()的唤醒并判断一下条件：
        - _connectQueue是否不为空？
        - vipUserDeque是否为空？即没有vip用户在排队
        - commonUserDeque.front()是不是自己？
        
        若都满足，则让该用户申请走连接

        **注：由于生产者新生产的连接只供给vip用户使用，故这里申请走的连接只能是用户主动析构或用户超时未处理回收到_connectQueue中的连接。**

    - 对于vip用户，如果判断_connectQueue为空，则先确定——总的连接数是否大于等于maxSize，如果没有，则按照连接池项目的申请方法正常申请。如果超过了，就得像普通用户一样排队，但vip用户依然有优先的排队通道：
        - _connectQueue是否不为空？
        - vipUserDeque.front()是不是自己？

    ### 当总连接数小于maxSize,大于initSize时:
    - ConnectionPool::getConnecion()中:
        ```cpp
        _produceForVip = true;  
		cv.wait(lock, [&]() -> bool {
					return _designedForVip == true;
				}
			);
		_designedForVip = false;
        ```
        先把_produceForVip置为true,只有为true的时候，生产者线程才能被唤醒。然后进入等待状态。

    - ConnectionPool::produceConnectionTask()中:
        ```cpp
        cv.wait(lock, [&]() -> bool {
				return _produceForVip == true;
			}
		);
        ```
        ```cpp
        _produceForVip = false;
		_designedForVip = true;

		cv.notify_all(); 
        ```
<br>

- ### 线程池先创建，并启动生产者线程。随后用户开始申请连接，当需要生产者为vip用户创建新的连接时：
    ```cpp
    cv.notify_all();  
    cv.wait(lock, 
        [&]() -> bool {
            return (!_connectQueue.empty()) 
                    && (_designedForVip == 2);
        }
    );
    ```
    先notify_all()，则生产者判断条件成立后由等待进入阻塞：

    ```cpp
    cv.wait(lock, 
    [&]() -> bool {
			return _produceForVip == true;
		}
	);
    ```
    而此刻阻塞的还有其他vip用户也在阻塞，于是当前用户线程想要通知生产者生产线程时，实际上是生产者和其他用户一起抢一个锁。这就导致，如果其他用户抢到锁后，也发起了一个notify通知生产者生产，这时已经有两个用户需要了，但接下来只生产一个，甚至接下来的锁还是被其他用户抢了。


    ### 设置一个标志_priorUser来限定：



    ```cpp
    unique_lock<mutex> lock(_queueMutex);
	cv.wait(lock,
		[&]() -> bool {
			return _priorUser == true;
		}
	);
	_priorUser = false;
    ```
    
    第一个用户进来时，上锁，其他用户阻塞在临界区外，判断条件成立，依然持有锁，然后_priorUser置为false。它申请完连接，就把_priorUser置回true，离开作用域后解锁。其他用户照常进行。

    第二个用户进来时，上锁，其他用户阻塞在临界区外，判断条件成立，依然持有锁，然后_priorUser置为false。它发现没有空闲连接了，就通知生产者生产连接，因为此时_priorUser为false，先notify_all()使生产者从等待进入阻塞，然后wait等待并释放锁，此时如果用户抢到了锁，它会进入wait并判断条件不成立，于是原地等待让出锁，故最终锁还是会交到生产者线程手上。

<br>

- ### 线程之间的同步通信

    在整个项目中，涉及到线程同步通信(wait, notify)的地方包括：

    1. 在获取连接时，为了更好的控制用户线程进出代码临界区，于是通过标签_priorUser来实现，最开始_priorUser为true：
        ```cpp
        cv.wait(lock,
            [&]() -> bool {
                return _priorUser == true;
            }
        );
        _priorUser = false;
        ```
        当该用户线程申请到了连接之后，就把_priorUser置回true，并通知其他用户线程进入临界区进行申请的后续操作。
        ```cpp
        _priorUser = true;
	    _connectQueue.pop();  // 然后弹出
	    cv.notify_all();  // 消费后就通知
        ```

    2. 对于vip用户，申请连接时，如果没有空闲连接，但是_connectionCnt小于_maxSize，连接池可以为vip专门临时申请部分连接使用：
        ```cpp
        _produceForVip = true;
		_designedForVip = 1;
		cv.notify_all();  
		cv.wait(lock, 
			[&]() -> bool {
				return (!_connectQueue.empty()) 
					&& (_designedForVip == 2);
			}
		);
		_designedForVip = 0;
        ```
        这里的_produceForVip一开始是false，置为true并notify_all后，生产者线程就会被唤醒，并在生产完连接后被生产者置回false。
        ```cpp
		cv.wait(lock, 
            [&]() -> bool {
				return (_produceForVip == true)
					&& (_designedForVip == 1);
			}
		);
        ```
        这里的_designedForVip的取值为0,1,2，为了更加准确的让生产者线程和vip用户线程进行通信。

    3. 对于vip用户，申请连接时，如果没有空闲连接，且_connectionCnt大于等于_maxSize，就需要进行排队。
        ```cpp
        vipUserDeque.push_back(dynamic_cast<VipUser*>(_abUser));		
        _priorUser = true;
        cv.notify_all();
        cv.wait(lock, 
            [&]() -> bool {	
                return (!_connectQueue.empty())
                        && vipUserDeque.front() == _abUser;
            }
        );			
        vipUserDeque.pop_front();
        ```
        notify_all后，它会进入等待状态，把锁交由其他线程(但生产者线程不符合唤醒条件，因此还在沉睡，只有其他临界区外的用户)。再次唤醒是需要满足以下的条件：
            
        - _connectQueue不能为空，即必须有空闲连接，不然继续等。
        - 它自己是队头，不然得轮到前面排队的先申请。

    4. 对于普通用户，申请连接时，如果没有空闲连接，需要进行排队。
        ```cpp
        commonUserDeque.push_back(dynamic_cast<CommonUser*>(_abUser));
		_priorUser = true;
        cv.notify_all();
		cv.wait(lock, 
            [&]() -> bool {
				return (!_connectQueue.empty())
					&& vipUserDeque.empty()
					&& commonUserDeque.front() == _abUser
					&& (_designedForVip == 0);
			}
		);
		commonUserDeque.pop_front();
        ```
        同理，但是普通用户的唤醒条件更加多：
        - connectQueue不能为空。
        - vipUserDeque必须为空，即如果vip用户都在排队，就轮不到普通用户。
        - 它是队头。
        - _designedForVip == 0，这个标志平时为0，当vip用户准备让生产者生产专属线程时，这个标志会被置为1；生产者生产完线程后，这个标志会被置为2。如果不设置2这个状态(生产者生产完后置为0)，那么这个专门为vip用户生产的连接可能就被普通用户抢了。

<br>

- ### 生产者线程意外被唤醒

    <img src='img/1.png'>

    并且输出显示两个标志符号都不满足生产者线程唤醒的条件：
    ```cpp
    while (!_connectQueue.empty())
	{
		cv.wait(lock, [&]() -> bool {
				// 只响应在可创建条件下的vip用户的创建请求
                cout << "_produceForVip: " << _produceForVip << " "
		            << "_designedForVip: " << _designedForVip << endl;
				return (_produceForVip == true)
					&& (_designedForVip == 1);
			}
		);
	}
    ```
    这一块原本的流程大致为：输出排队的所有用户 -> _priorUser置为true，准备把互斥锁交给其他用户 -> cv.notify_all() -> cv.wait()释放锁，其他用户获得锁，然后继续.....

    但根据输出，并没有生产者在等待状态进行判断的迹象(没有打印信息)，而是直接从为vip用户生产新的线程，说明生产者不是从wait中醒来的，而是在被阻塞在代码临界区之外，在vip用户进入等待的同时释放互斥锁，生产者获得了这把锁，进入判定，发现此时_connectQueue是空的，便直接生产连接去了。
    
    于是生产完连接，开始新的循环，此时_connectQueue不在为空，就进入等待，判断一次条件，打印信息。这是被唤醒的vip用户可能是排队在最前面的，也可能是还没进来排队的，它们都满足被唤醒的条件。申请完连接后就notify_all()，可能生产者抢到，就会在判断一次条件，打印输出；如果是用户抢到就没有。

    <img src='img/2.png'>

    ### 问题已经找到，为什么生产者会有小概率被阻塞在临界区之外呢？
    
    ~~因为最后生产者通知时，标志_priorUser的值为true，这个条件会把还在排队的用户变为阻塞状态，去和生产者竞争互斥锁，其他排队的用户可能会很多，这也说明了这个问题的发生概率比较小。~~

    第_maxSize个用户申请连接，也是目前最后一个生产者可以给他生产额外连接的用户。用户指向notify_all()，此时_priorUser为false，因此其他用户都醒不来，生产者必定拿到这个锁。生产者生产完连接后，也执行notify_all()，这个时候_priorUser依然是false,notify_all()也只唤醒了发出连接申请的用户线程，从等待变为阻塞，然后生产者结束当前循环，释放互斥锁，准备开启一个新的循环。这个时候，有两种情况：
    - <u>生产者抢到了这把互斥锁</u>:
    
        因为刚生产的连接还在_connectQueue还未被取走，故生产者判定_connectQueue.empty()不成立，进入等待状态，交出锁的使用权，自然而然的刚才的用户就获得了锁，取走连接，_priorUser置为true，_designedForVip置为0，这样生产者的唤醒条件不满足，其他用户的唤醒条件满足了，然后notify_all()，于是下一个用户进来申请，这种情况没有问题。

    - <u>刚才的用户抢到了这把互斥锁</u>:

        生产者开启新循环后被阻塞在临界区之外，而用户依旧正常取走了连接, _priorUser被置为true, 后notify_all(), 把所有等待状态的用户变为阻塞状态，即接下来生产者要和所有还在等在申请连接的用户抢一把互斥锁：
        
        - 如果抢到了，因为_connectQueue是空的，因此生产者会生产连接，但是此时总的连接数_connectionCnt已经等于_maxSize了，是不允许生产者继续生产的.....

        - 如果没抢到，即被其他用户抢到了，_priorUser置为true(但本来就是true),执行notify_all()，依然将所有等待状态的用户转为阻塞状态，和生产者一起抢互斥锁，就回到了上面的情况。
    
        如果生产者一直没有抢到锁，那么也不会有问题，但如果会有小概率生产者在某一次判断中抢到了，便会引发情况一的问题。

    ### 解决方法

    只要在第一种互斥锁争抢中，始终让生产者抢到这把锁。由于生产者唤醒需要两个条件：
    - _produceForVip == true
    - _designedForVip == 1

    便在生产完连接后不先把_designedForVip置为2，这样生产者执行notify_all()之后用户不会唤醒而进入阻塞状态。同时等生产者在新循环中获得互斥锁后再把_designedForVip置为2，然后notify_all()，这时用户才被唤醒进入阻塞状态。

    ```cpp
    for (;;)
	{
		unique_lock<mutex> lock(_queueMutex);

		while (!_connectQueue.empty())
		{
        // ################# 新的代码段 #################
			_designedForVip = 2;
			cv.notify_all();
        // #############################################
			cv.wait(lock, [&]() -> bool {
					return (_produceForVip == true)
						&& (_designedForVip == 1);
				}
			);
		}
		cout << "为vip用户创建新的连接...."  << endl;
		Connection* p = new Connection();
		p->connect(_ip, _port, _username, _password, _dbname);
		p->refreshAliveTime();  // 刷新计时
		_connectQueue.push(p);
		_connectionCnt++;
		_produceForVip = false;
	
		/*_designedForVip = 2;*/

		cv.notify_all();  // 通知消费者线程，可以消费连接了
	}
    ```

   ### 但是上面的方法又影响了普通用户的使用：
    由于连接池启动的时候，生产者线程会进入临界区，判断条件后进入等待状态，会把_designedForVip置为2，而这不满足普通用户的唤醒条件：
    ```cpp
    return (!_connectQueue.empty())
			&& vipUserDeque.empty()
			&& commonUserDeque.front() == _abUser
			&& (_designedForVip == 0);
    ```
    因此即使之前申请的连接被回收了，排队的用户也无法被唤醒。

    _designedForVip的设置之处是为了应对以下场景：一个普通用户进来申请——没有空闲连接：等待。之后一个vip用户进来申请连接——没有空闲连接，但生产者可以为他生产。它先进入等待状态，生产者生产完毕后会通知，vip用户申请连接等待生产者生产时的唤起条件：
    ```cpp
    [&]() -> bool {
		return (!_connectQueue.empty()) 
			&& (_designedForVip == 2);
	}
    ```
    普通用户排队等待的唤起条件：
    ```cpp
    [&]() -> bool {
        return (!_connectQueue.empty())
            && vipUserDeque.empty()
            && commonUserDeque.front() == _abUser
            && (_designedForVip == 0);
	}
    ```
    如果没有_designedForVip，且等待的用户只有一个(那他就是队头)，同时vip用户既然还能让生产者生产连接，就说明没有vip用户在排队，因此，普通用户和vip用户都会被唤醒，可能存在vip用户让生产者生产的连接被普通用户抢走。

    超时回收连接并放回连接池中：
    ```cpp
    _Connection.reset();
	_Connection = nullptr;
    // ################# 新的代码段 #################
	_connectionPool->_designedForVip = 0;
    // #############################################
    ```

<br>

- ### 退出排队机制的引入

    用户可以在自己排队的时候手动选择退出排队，这时，它的名字将在对应的连接池对应的排队队列中删去。

    - 首先把获取连接设置在一个独立的线程中：
        ```cpp
        thread t0(
		    [&]() -> void {
			    _Connection = _pConnectPool->getConnection(this);
		    }
	    );
        ```
        如果不设置为一个单独的线程，那么顺序执行，就会等到getConnection()执行完成才往下执行，即已经申请到内存了。如果用户发出排队退出的请求，则需要直接终止线程t0。

        这样的话，超时重连线程的启动也需要进行一定的修改：timeoutRecycleConnect()

        首先，~~_Connection不为nullptr时，肯定是已经申请到了连接；_Connection为nullptr，且_waiting为true，说明用户在排队；如果_Connection为nullptr，且_waiting为false？~~

        不能只用_Connection去判断，因为对于vip用户，_Connection为nullptr的情况下可能是在等生产者生产连接，而不是在排队。因此应该用_waiting来判断。_waiting只有在开始排队前才会设置为true，而排队结束后_waiting的值也不影响其他。
        ```cpp
        while (_Connection == nullptr)
        {
            // 如果用户确实在等待
            if (_waiting)
            {
                int choice = rand() % 20 + 1;
                // 模拟5%的概率用户选择退出排队
                if (choice <= 1) 
                {
                    ......
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            // 如果申请到连接了还没有退出排队，while循环就会停止，往下执行，开始超时回收连接的判定。
        }
        if (_Connection != nullptr)
        {
            ......
        }
        ```

        同时，当执行退出排队相关函数时，也要把getConnection()和timeoutRecycleConnect()对应的线程关闭，这可以通过一些标志来实现。例如，当设置label=true时终止getConnection()，可以在唤醒条件中如:
        ```cpp
        [&]() -> bool {
            return (label == true) ||
                   ((!_connectQueue.empty()
                && vipUserDeque.front() == _abUser));
        }
        if(label)
        {
            return;
        }
        ```
        按照从左到右的顺序，如果label==true，那么或的另一个条件就不需要判断了，或的结果一定是true，因此可以把进程唤醒，同时避免了删除唯一一个排队的用户后需要访问UserDeque.front()而报错。

        但是还有一个棘手的问题：用户端的主进程的notify_all()如何准确找到它对应的
        getConnection()子进程——**把标志设置为成员变量**。

        最终整个退出排队的执行逻辑：

        - User: 用户在一个独立的线程中，发起退出排队请求，并把_terminate置为true，然后cv.notify_all()

        - ConnectionPool: 那些_terminate=true的用户全部被唤醒，其中一个获得了互斥锁

        - ConnectionPool: 如果判断是因为退出排队(_terminate=true)而被唤醒，则调用deleteFromDeque()，从对应的deque队列中删除该用户，并且直接返回nullptr，不再为用户申请连接


        


<br>

- ### 信号量(Semaphore)替换_connectQueue中的队列queue的可能性

    计数信号量：
    ```cpp
    std::counting_semaphore<_initSize> _connectSemaphore(_initSize);
    ```
    当线程或进程需要访问共享资源时，它会尝试获取信号量。如果信号量的计数值大于0，则将其减1并允许线程或进程继续执行；**如果信号量的计数值为0，则线程或进程将被阻塞**，**直到信号量的计数值大于0**。

    计数信号量本身只是个计数，并不能获取具体资源本身如connection。但是可以和_connectQueue一起使用:
    ```cpp
    // 判断申请连接的是普通用户还是vip用户
	if (dynamic_cast<CommonUser*>(_abUser) != nullptr)
    {
        /*
            这里信号量的计数对应_connectionQueue.size(), 
            当_connectQueue的计数为0，即队列内没有空闲连接时，
            线程会被阻塞在这里，
            直到_connectQueue的计数大于0，有空闲连接时。
        */
        _connectSemaphore.acquire();

        shared_ptr<Connection> sp(_connectQueue.front(),
    		[&](Connection* pcon) { 
			    unique_lock<mutex> lock(_queueMutex);
			    // 刷新计时
			    pcon->refreshAliveTime();
			_   connectQueue.push(pcon);
		    }
	    );   // 取队头

	    _priorUser = true;
	    _connectQueue.pop();  // 然后弹出
	    cv.notify_all();  // 消费后就通知

	    return sp;
    }
    ```
