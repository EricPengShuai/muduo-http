#pragma once

#include <algorithm>
#include <string>
#include <vector>

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode

// 网络库底层地缓冲器类型定义
class Buffer {
  public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend) {}

    ~Buffer();

    /**
     * kCheapPrepend | reader | writer |
     * writerIndex_ - readerIndex_
     */
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }

    /**
     * kCheapPrepend | reader | writer |
     * buffer_.size() - writerIndex_
     */
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }

    /**
     * kCheapPrepend | reader | writer |
     * readerIndex_
     */
    size_t prependableBytes() const {  // 前面空闲的缓冲区
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const { return begin() + readerIndex_; }

    void retrieveUntil(const char *end)
    {
        retrieve(end - peek());
    }

    //!NOTE: 相当于更新 readerIndex/writerIndex, onMessage string <-- Buffer
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            readerIndex_ += len;  // 读取一部分
        } else {
            retrieveAll();  // 读取全部
        }
    }

    // 全部读完，则直接将可读缓冲区指针移动到写缓冲区指针那
    void retrieveAll() {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // DEBUG使用，提取出 string 类型，但是不会置位
    std::string GetBufferAllAsString()
    {
        size_t len = readableBytes();
        std::string result(peek(), len);
        return result;
    }

    // 将 onMessage 函数上报的 Buffer 数据转换成 string 类型的数据返回
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());  // 应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len) {
        std::string res(peek(), len);
        retrieve(len);  // 上一句把缓冲区可读的数据已经读取出来，这里需要复位缓冲区
        return res;
    }

    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
    }

    // string::data() 转换成字符数组，但是没有 '\0'
    void append(const std::string &str)
    {
        append(str.data(), str.size());
    }

    // 把 data 写入 writerIndex 开始的地址
    void append(const char *data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;  // 更新 writeIndex
    }

    // 在 buffer 找到 "\r\n" 的位置并返回，如果没有就返回 NULL
    const char* findCRLF() const
    {
        // FIXME: replace with memmem()?
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    char *beginWrite() { return begin() + writerIndex_; }

    const char *beginWrite() const { return begin() + writerIndex_; }

    ssize_t readFd(int fd, int *saveErrno);   // 从 fd 上读取数据
    ssize_t writeFd(int fd, int *saveErrno);  // 通过 fd 发送数据

  private:
    char *begin() {
        return &*buffer_.begin();  // vector 底层数组首元素的地址，也就是数组的起始地址
    }

    const char *begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len) {
        /**
         *  kCheapPrepend | reader | writer |
         *  kCheapPrepend   |        len       |
         */
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) { // 整个 buffer 不够用
            buffer_.resize(writerIndex_ + len);
        } else { // 整个 buffer 够用，将后面移动到前面继续分配
            size_t readable = readableBytes();
            // 把已读的部分挪至前面
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);

            // 更新 readIndex 和 writeIndex
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

  private:
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

    static const char kCRLF[]; // "\r\n"
};
