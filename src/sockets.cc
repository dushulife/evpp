#include "evpp/inner_pre.h"

#include "evpp/libevent_headers.h"
#include "evpp/sockets.h"

namespace evpp {
    EVPP_EXPORT std::string strerror(int e) {
#ifdef H_OS_WINDOWS
        LPVOID lpMsgBuf = NULL;
        ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&lpMsgBuf, 0, NULL);
        if (lpMsgBuf) {
            std::string s = (char*)lpMsgBuf;
            LocalFree(lpMsgBuf);
            return s;
        }
        return std::string();
#else
        char buf[1024] = {};
        return std::string(strerror_r(e, buf, sizeof buf));
#endif
    }


    int CreateNonblockingSocket() {
        int serrno = 0;
        int on = 1;

        /* Create listen socket */
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        serrno = errno;
        if (fd == -1) {
            LOG_ERROR << "socket error " << strerror(serrno);
            return INVALID_SOCKET;
        }

        if (evutil_make_socket_nonblocking(fd) < 0)
            goto out;

#ifndef H_OS_WINDOWS
        if (fcntl(fd, F_SETFD, 1) == -1) {
            serrno = errno;
            LOG_FATAL << "fcntl(F_SETFD)" << strerror(serrno);
            goto out;
        }
#endif

        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&on, sizeof(on));
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
        return fd;
    out:
        EVUTIL_CLOSESOCKET(fd);
        return INVALID_SOCKET;
    }

    struct sockaddr_in ParseFromIPPort(const char* address/*ip:port*/) {
        struct sockaddr_in addr;
        std::string a = address;
        size_t index = a.find_first_of(':');
        if (index == std::string::npos) {
            LOG_FATAL << "Address specified error [" << address << "]";
        }

        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(::atoi(&a[index + 1]));
        a[index] = '\0';
        if (::inet_pton(AF_INET, a.data(), &addr.sin_addr) <= 0) {
            int serrno = errno;
            LOG_FATAL << "sockets::fromIpPort " << strerror(serrno);
        }

        //TODO add ipv6 support
        return addr;
    }

    struct sockaddr_in GetLocalAddr(int sockfd) {
        struct sockaddr_in laddr;
        memset(&laddr, 0, sizeof laddr);
        socklen_t addrlen = static_cast<socklen_t>(sizeof laddr);
        if (::getsockname(sockfd, sockaddr_cast(&laddr), &addrlen) < 0) {
            LOG_ERROR << "GetLocalAddr:" << strerror(errno);
            memset(&laddr, 0, sizeof laddr);
        }
        return laddr;
    }

    std::string ToIPPort(const struct sockaddr_storage* ss) {
        std::string saddr;
        int port = 0;
        if (ss->ss_family == AF_INET) {
            struct sockaddr_in* addr4 = const_cast<struct sockaddr_in*>(sockaddr_in_cast(ss));
            char buf[INET_ADDRSTRLEN] = {};
            const char* addr = ::inet_ntop(ss->ss_family, &addr4->sin_addr, buf, INET_ADDRSTRLEN);
            if (addr) {
                saddr = addr;
            }
            port = ::ntohs(addr4->sin_port);
        } else if (ss->ss_family == AF_INET6) {
            struct sockaddr_in6* addr6 = const_cast<struct sockaddr_in6*>(sockaddr_in6_cast(ss));
            char buf[INET6_ADDRSTRLEN] = {};
            const char* addr = ::inet_ntop(ss->ss_family, &addr6->sin6_addr, buf, INET6_ADDRSTRLEN);
            if (addr) {
                saddr = addr;
            }
            port = ::ntohs(addr6->sin6_port);
        } else {
            LOG_ERROR << "unknown socket family connected";
            return std::string();
        }

        if (!saddr.empty()) {
            char buf[16] = {};
            snprintf(buf, sizeof(buf), "%d", port);
            saddr.append(":", 1).append(buf);
        }

        return saddr;
    }

    void SetKeepAlive(int fd) {
        int on = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&on, sizeof(on));
    }


}

#ifdef H_OS_WINDOWS
int readv(int sockfd, struct iovec *iov, int iovcnt) {
    DWORD readn = 0;
    DWORD flags = 0;
    if (::WSARecv(sockfd, iov, iovcnt, &readn, &flags, NULL, NULL) == 0) {
        return readn;
    }
    return -1;
}
#endif