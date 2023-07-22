#pragma once

#include "TcpServer.h"
#include "noncopyable.h"
#include "Logger.h"

#include <list>
#include <memory>
#include <string>

class HttpRequest;
class HttpResponse;

class HttpServer : noncopyable
{
public:
    using HttpCallback = std::function<void (const HttpRequest&, HttpResponse*)>;

    HttpServer(EventLoop *loop,
            const InetAddress& listenAddr,
            int idleSeconds,
            const std::string& name,
            TcpServer::Option option = TcpServer::kNoReusePort);
    
    EventLoop* getLoop() const { return server_.getLoop(); }

    void setHttpCallback(const HttpCallback& cb)
    {
        httpCallback_ = cb;
    }
    
    void start();

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr &conn,
                    Buffer *buf,
                    Timestamp receiveTime);
    void onRequest(const TcpConnectionPtr&, const HttpRequest&);

    TcpServer server_;
    HttpCallback httpCallback_;

    /* 定时器相关 */
    void onTimer();

    using WeakTcpConnectionPtr = std::weak_ptr<TcpConnection>;
    using WeakConnectionList = std::list<WeakTcpConnectionPtr>; // weak_ptr conn list

    struct Node {
        Timestamp lastReceiveTime;
        WeakConnectionList::iterator position;
    };
    using NameNode = std::unordered_map<std::string, Node>; // connName --> Node

    int idleSeconds_;
    WeakConnectionList connectionList_;
    NameNode nodeMap_;
};
