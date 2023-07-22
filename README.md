## MUDUO-HTTP
> 基于 mymuduo 再次重构，添加 HTTPServer 例子

### 1 改动点

#### 1.1 Timerstamp
- 使用 gettimeofday(&tv, NULL) 获取 microSecondsSince_
- 重载 operate<(Timestamp lhs, Timestamp rhs) 以及 operator==(Timestamp lhs, Timestamp rhs)
- 提供 addTime(Timestamp timestamp, double seconds) -> Timestamp
- 获取时间差 timeDifference(Timestamp high, Timestamp low) -> double

#### 1.2 Timer/TimerQueue
- set 红黑树存储 pair<Timestamp, Timer*>, 按照 Timestamp 从小到大排序
- 通过 timerfd_create 创建 timerfd，通过 Channel 注册到 Poller 上并 enableReading
- 只提供一个 public 接口 addTimer(TimerCallback cb, Timestamp when, double interval)
- resetTimerfd 通过 timerfd_settime 重置 timer 的到期时间

#### 1.3 EventLoop
- 新增 std::unique_ptr<TimerQueue> timerQueue_
- 定时器相关方法：runAt, runAfter, runEvery

#### 1.4 TcpConnection
- 新增关闭连接的方法 forceClose(), forceCloseWithDelay(double seconds)
- 提供 send(Buffer *buf) 重载方法

#### 1.5 CHANNELTYPE

添加 enableReading 的重载类型，打印 channel type 的具体信息，具体体现在：
- Channel::enableReading(const std::string &type)
- Channel::update(const std::string &type)
- EventLoop::updateChannel(Channel *channel, const std::string &buf)
- Poller::updateChannel(Channel *channel, const std::string &type)
- EPollPoller::updateChannel(Channel *channel, const std::string &type)

**关于 enableReading()**
- wakeupChannel_->enableReading("wakeupChannel")
- timerfdChannel_.enableReading("timerfdChannel")
- acceptChannel_.enableReading("acceptChannel")
- connChannel->eenableReading("connChannel")

### 2 例子

#### 2.1 EchoServer
最基本的回射服务器例子，提供 onConnection 和 onMessage 方法

#### 2.2 TimerServer
在回射服务器基础上新增计数器回调方法
```cpp
using WeakTcpConnectionPtr = std::weak_ptr<TcpConnection>;
using WeakConnectionList = std::list<WeakTcpConnectionPtr>; // weak_ptr conn list

struct Node { // 保存活跃的连接信息
    Timestamp lastReceiveTime;
    WeakConnectionList::iterator position;
};
using NameNode = std::unordered_map<std::string, Node>; // connName --> Node

TcpServer server_;
int idleSeconds_;
WeakConnectionList connectionList_; // weak_ptr list 管理活跃连接
NameNode nodeMap_; // 哈希表管理 connName --> Node
```

#### 2.3 HTTPServer

**HttpContext**
- 解析请求头，状态机设计思想：kExpectRequestLine -> kExpectHeaders -> kExpectBody -> kGotAll
- processRequestLine: 只支持 GET 请求
- parseRequest: 通过 processRequestLine 解析 GET 请求之后，然后处理各个 Headers，保存在 request_.headers 哈希表中

**HttpRequest**
- 定义 HTTP method 以及 version
- 提供 setMethod 以及 methodString
- 主要维护 std::unordered_map<std::string, std::string> headers_

**HttpResponse**
- 定义 HTTP status code
- 维护 closeConnection_ 是否 keep alive
- 提供 appendToBuffer(Buffer* output) 填充 outputBuffer_

**HttpServer**
- 提供常规的 onConnection 以及 onMessage 作为 callback，其中需要维护计时器列表，类似与 [TimerSrever](#### 2.2 TimerServer)
- 提供 HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req) 方法 conn->send(buf)
  - 其中需要用户自定义 std::function<void (const HttpRequest&, HttpResponse*)> httpCallback_ 方法
  - 根据 response.closeConnection 是否 shutdown
- 定时器模块 onTimer 处理到期连接，参考 [TimerSrever](#### 2.2 TimerServer)

**main**
主要提供 void onRequest(const HttpRequest& req, HttpResponse* resp) 作为 HttpServer 的 callback，处理业务逻辑

### 3 参考

- chenshuo muduo 源码: https://github.com/chenshuo/muduo
- C++11 改写 muduo: https://github.com/Shangyizhou/A-Tiny-Network-Library
- 施磊 muduo 剖析: https://github.com/EricPengShuai/muduo

