#include "Logger.h"
#include "EventLoop.h"
#include "TcpServer.h"

#include <list>
#include <unordered_map>
#include <stdio.h>
#include <unistd.h>

// RFC 862
class EchoServer
{
public:
    EchoServer(EventLoop *loop,
               const InetAddress &listenAddr,
               int idleSeconds);

    void start()
    {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr &conn);

    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp time);

    void onTimer();

    using WeakTcpConnectionPtr = std::weak_ptr<TcpConnection>;
    using WeakConnectionList = std::list<WeakTcpConnectionPtr>; // weak_ptr conn list

    struct Node : public muduo::copyable
    {
        Timestamp lastReceiveTime;
        WeakConnectionList::iterator position;
    };
    using NameNode = std::unordered_map<std::string, Node>; // connName --> Node

    TcpServer server_;
    int idleSeconds_;
    WeakConnectionList connectionList_;
    NameNode nodeMap_;
};

EchoServer::EchoServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       int idleSeconds)
    : server_(loop, listenAddr, "EchoServer"),
      idleSeconds_(idleSeconds)
{
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, _1, _2, _3));

    loop->runEvery(5.0, std::bind(&EchoServer::onTimer, this));
}

void EchoServer::onConnection(const TcpConnectionPtr &conn)
{
    std::string up_down = conn->connected() ? "UP" : "DOWN";
    LOG_INFO("EchoServer - %s -> %s is %s", 
        conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str(), up_down.c_str());

    if (conn->connected())
    {
        Node node;
        node.lastReceiveTime = Timestamp::now();
        connectionList_.push_back(conn); // 添加 conn 到 connectionList_
        node.position = --connectionList_.end();
        nodeMap_[conn.name()] = node;
    }
    else
    {
        assert(nodeMap_.count(conn.name()));
        const Node &node = nodeMap_[conn.name()];
        nodeMap_.erase(conn.name());
        connectionList_.erase(node.position); // 移除 conn
    }
}

void EchoServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp time)
{
    string msg(buf->retrieveAllAsString());

    LOG_INFO("%s echo %ul bytes at %s", conn->name().c_str(), msg.size(), time.toString().c_str());
    conn->send(msg);

    assert(nodeMap_.count(conn.name()));
    Node *node = &nodeMap_[conn.name()];
    node->lastReceiveTime = time;

    connectionList_.splice(connectionList_.end(), connectionList_, node->position); // 将当前 node 移到 list 末尾
    assert(node->position == --connectionList_.end());
}

void EchoServer::onTimer()
{
    Timestamp now = Timestamp::now();
    for (WeakConnectionList::iterator it = connectionList_.begin();
         it != connectionList_.end();)
    {
        TcpConnectionPtr conn = it->lock();
        if (conn)
        {
            Node *n = &nodeMap_[conn.name()];
            double age = timeDifference(now, n->lastReceiveTime);
            if (age > idleSeconds_)
            {
                if (conn->connected())
                {
                    conn->shutdown();
                    LOG_INFO("shutting down %s", conn->name().c_str());
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
        else
        {
            LOG_WARN("Expired");
            if (nodeMap_.count(it->name())) {
                nodeMap_.erase(it->name());
            }
            it = connectionList_.erase(it);
        }
    }
}

int main(int argc, char *argv[])
{
    EventLoop loop;
    InetAddress listenAddr(8080);
    int idleSeconds = 10;
    if (argc > 1)
    {
        idleSeconds = atoi(argv[1]);
    }
    LOG_INFO("pid = %d, idle seconds = %d", getpid(), idleSeconds);
    EchoServer server(&loop, listenAddr, idleSeconds);
    server.start();
    loop.loop();
}
