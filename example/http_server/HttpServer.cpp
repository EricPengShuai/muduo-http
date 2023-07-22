#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpContext.h"

#include <functional>
#include <cassert>

using namespace std::placeholders;

/**
 * 默认的http回调函数
 * 设置响应状态码，响应信息并关闭连接
 */
void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
}

HttpServer::HttpServer(EventLoop *loop,
                      const InetAddress &listenAddr,
                      int idleSeconds,
                      const std::string &name,
                      TcpServer::Option option)
  : server_(loop, listenAddr, name, option)
  , httpCallback_(defaultHttpCallback)
  , idleSeconds_(idleSeconds)
{
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(4);

    // 每过 5s 检查一下 conn 是否过期
    loop->runEvery(5.0, std::bind(&HttpServer::onTimer, this)); 
}

void HttpServer::start()
{
    LOG_INFO("HttpServer[%s] starts listening on %s", server_.name().c_str(), server_.ipPort().c_str());
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        LOG_INFO("New connection arrived");

        Node node;
        node.lastReceiveTime = Timestamp::now();
        connectionList_.push_back(conn); // 添加 conn 到 connectionList_
        node.position = -- connectionList_.end();
        nodeMap_[conn->name()] = node;
    }
    else 
    {
        LOG_INFO("Connection closed");

        assert(nodeMap_.count(conn->name()));
        const Node &node = nodeMap_[conn->name()];
        nodeMap_.erase(conn->name());
        connectionList_.erase(node.position); // 移除 conn
    }
}

// 有消息到来时的业务处理
void HttpServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
    std::unique_ptr<HttpContext> context(new HttpContext);

    /* 定时器相关 */
    assert(nodeMap_.count(conn->name()));
    Node *node = &nodeMap_[conn->name()];
    node->lastReceiveTime = receiveTime;

    connectionList_.splice(connectionList_.end(), connectionList_, node->position); // 将当前 node 移到 list 末尾
    assert(node->position == -- connectionList_.end());

#if 0
    // 打印请求报文
    std::string request = buf->GetBufferAllAsString();
    std::cout << request << std::endl;
#endif

    // 进行状态机解析
    // 错误则发送 BAD REQUEST 半关闭
    if (!context->parseRequest(buf, receiveTime))
    {
        LOG_INFO("ParseRequest failed!");
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }

    // 如果成功解析
    if (context->gotAll())
    {
        LOG_INFO("ParseRequest success!");
        onRequest(conn, context->request());
        context->reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
    const std::string& connection = req.getHeader("Connection");

    // 判断长连接还是短连接
    bool close = connection == "close" ||
        (req.version() == HttpRequest::kHTTP10 && connection != "Keep-Alive");

    // TODO:这里有问题，但是强制改写了
    // close = true;

    // 响应信息
    HttpResponse response(close);
    // httpCallback_ 由用户传入，怎么写响应体由用户决定
    // 此处初始化了一些response的信息，比如响应码，回复OK
    httpCallback_(req, &response);

    Buffer buf;
    response.appendToBuffer(&buf);
    conn->send(&buf);
    
    if (response.closeConnection())
    {
        conn->shutdown();
    }
}

void HttpServer::onTimer()
{
    Timestamp now = Timestamp::now();
    for (WeakConnectionList::iterator it = connectionList_.begin();
         it != connectionList_.end();)
    {
        TcpConnectionPtr conn = it->lock();
        if (conn)
        {
            Node *n = &nodeMap_[conn->name()];
            double age = timeDifference(now, n->lastReceiveTime);
            if (age > idleSeconds_)
            {
                if (conn->connected())
                {
                    conn->shutdown();
                    LOG_INFO("[HttpServer::onTimer()] shutting down %s", conn->name().c_str());
                    conn->forceCloseWithDelay(3.5); //!NOTE > round trip of the whole Internet.
                }
            }
            else if (age < 0)
            {
                LOG_WARN("Time jump");
                n->lastReceiveTime = now;
            }
            else // age < idleSeconds 说明还没有到期直接退出
            {
                break;
            }
            ++it;
        }
        else // weak_ptr 提升失败
        {
            LOG_WARN("Expired");
            // if (nodeMap_.count(it->name())) {
            //     nodeMap_.erase(it->name());
            // }
            it = connectionList_.erase(it);
        }
    }
}
