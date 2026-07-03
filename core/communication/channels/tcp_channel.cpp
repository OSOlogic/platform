/**
 * @file tcp_channel.cpp
 * @author Diego Arcos Sapena
 * @brief TCP/IP communication channel (code)
 * @version 1.0.0
 * @date 2024/07/25
 *
 * @copyright Copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "tcp_channel.hpp"

#include <arpa/inet.h>  // For inet_pton
#include <errno.h>      // For errno and strerror
#include <fcntl.h>      // For fcntl
#include <netinet/ip.h>
#include <netinet/tcp.h>  // For TCP_NODELAY
#include <unistd.h>       // For close()

#include <cstring>

#include "../../common/debug.hpp"

TCP_Channel::TCP_Channel(const std::string &ip_address, int port)
    : _ip_address(ip_address), _port(port), _socket_fd(-1), _is_connected(false) {}

TCP_Channel::~TCP_Channel() {
  disconnect();
}

PlcErrorCodes TCP_Channel::connect() {
  std::lock_guard<std::mutex> lock(_mutex);
  DEBUG_STREAM("[TCP] Attempting to connect to " << _ip_address << ":" << _port);
  if (_is_connected) {
    DEBUG_STREAM("[TCP] Already connected to " << _ip_address << ":" << _port);
    return PlcErrorCodes::PLC_SUCCESS;
  }

  _socket_fd =
      socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);  // Non-blocking connect
  if (_socket_fd < 0) {
    log_error("TCP_Channel::connect", "Failed to create socket: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_TCP_SOCKET_CREATION);
    return PlcErrorCodes::ERROR_TCP_SOCKET_CREATION;
  }

  // Set socket options for low delay (optional, but good for real-time applications)
  int opt = 1;
  if (setsockopt(_socket_fd, IPPROTO_TCP, TCP_NODELAY, (const void *)&opt, sizeof(int)) < 0) {
    log_error("TCP_Channel::connect", "Failed to set TCP_NODELAY: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_TCP_SET_SOCKOPT_FAILED);
    // Non-fatal, continue
  }
  opt = IPTOS_LOWDELAY;
  if (setsockopt(_socket_fd, IPPROTO_TCP, IP_TOS, (const void *)&opt, sizeof(int)) < 0) {
    log_error("TCP_Channel::connect", "Failed to set IP_TOS: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_TCP_SET_SOCKOPT_FAILED);
    // Non-fatal, continue
  }

  sockaddr_in serv_addr;
  std::memset(&serv_addr, 0, sizeof(serv_addr));  // Clear the structure
  serv_addr.sin_family = AF_INET;                 // IPv4
  serv_addr.sin_port = htons(_port);              // Convert port to network byte order

  if (inet_pton(AF_INET, _ip_address.c_str(), &serv_addr.sin_addr) <= 0) {
    log_error("TCP_Channel::connect", "Invalid IP address: " + _ip_address,
              PlcErrorCodes::ERROR_TCP_INVALID_ADDRESS);
    close(_socket_fd);
    _socket_fd = -1;
    return PlcErrorCodes::ERROR_TCP_INVALID_ADDRESS;
  }

  int connect_res = ::connect(_socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (connect_res < 0 && errno != EINPROGRESS) {
    log_error("TCP_Channel::connect",
              "Connection failed immediately for " + _ip_address + ":" + std::to_string(_port) +
                  ". Error: " + strerror(errno),
              PlcErrorCodes::ERROR_TCP_CONNECTION_FAILED);
    close(_socket_fd);
    _socket_fd = -1;
    return PlcErrorCodes::ERROR_TCP_CONNECTION_FAILED;
  } else if (connect_res == -1 && errno == EINPROGRESS) {
    // Non-blocking connect in progress. Use select to wait for completion.
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(_socket_fd, &wset);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int select_res =
        select(_socket_fd + 1, NULL, &wset, NULL, &tv);  // Wait for the socket to be writable
    if (select_res <= 0) {                               // Timeout or error
      log_error("TCP_Channel::connect",
                "Connection timeout or error for " + _ip_address + ":" + std::to_string(_port) +
                    ". Select result: " + std::to_string(select_res) +
                    ", Error: " + strerror(errno),
                PlcErrorCodes::ERROR_TCP_CONNECTION_FAILED);
      close(_socket_fd);
      _socket_fd = -1;
      return PlcErrorCodes::ERROR_TCP_CONNECTION_FAILED;
    }

    // Check for socket error
    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(_socket_fd, SOL_SOCKET, SO_ERROR, (void *)&optval, &optlen) < 0 || optval != 0) {
      log_error("TCP_Channel::connect",
                "Socket error after select for " + _ip_address + ":" + std::to_string(_port) +
                    ". SO_ERROR: " + strerror(optval),
                PlcErrorCodes::ERROR_TCP_CONNECTION_FAILED);
      close(_socket_fd);
      _socket_fd = -1;
      return PlcErrorCodes::ERROR_TCP_CONNECTION_FAILED;
    }
  }

  _is_connected = true;
  DEBUG_STREAM("[TCP] Connected successfully to " << _ip_address << ":" << _port);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes TCP_Channel::disconnect() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_is_connected && _socket_fd != -1) {
    if (shutdown(_socket_fd, SHUT_RDWR) < 0) {  // Shutdown both read and write sides of the socket
      log_error("TCP_Channel::disconnect",
                "Shutdown socket failed: " + std::string(strerror(errno)),
                PlcErrorCodes::ERROR_TCP_CLOSE_FAILED);
    }
    if (close(_socket_fd) < 0) {  // Close the socket at SO level
      log_error("TCP_Channel::disconnect", "Close socket failed: " + std::string(strerror(errno)),
                PlcErrorCodes::ERROR_TCP_CLOSE_FAILED);
    }
    _socket_fd = -1;
    _is_connected = false;
    DEBUG_STREAM("[TCP] Disconnected from " << _ip_address << ":" << _port);
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes TCP_Channel::write(const void *buf, size_t n, ssize_t &bytes_written) {
  std::lock_guard<std::mutex> lock(_mutex);
  bytes_written = 0;
  if (!_is_connected || _socket_fd == -1) {
    log_error("TCP_Channel::write", "Not connected to TCP server.",
              PlcErrorCodes::ERROR_TCP_NOT_CONNECTED);
    return PlcErrorCodes::ERROR_TCP_NOT_CONNECTED;
  }
  if (buf == nullptr) {
    log_error("TCP_Channel::write", "Null buffer provided.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  ssize_t sent_bytes =
      send(_socket_fd, buf, n, MSG_NOSIGNAL);  // MSG_NOSIGNAL to prevent SIGPIPE. Avoids the core
                                               // from crashing if the peer closes the connection.
  if (sent_bytes < 0) {
    log_error("TCP_Channel::write", "Failed to send data. Error: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_TCP_WRITE_FAILED);
    return PlcErrorCodes::ERROR_TCP_WRITE_FAILED;
  }
  bytes_written = sent_bytes;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes TCP_Channel::read(void *buf, size_t n, ssize_t &bytes_read) {
  std::lock_guard<std::mutex> lock(_mutex);
  bytes_read = 0;
  if (!_is_connected || _socket_fd == -1) {
    log_error("TCP_Channel::read", "Not connected to TCP server.",
              PlcErrorCodes::ERROR_TCP_NOT_CONNECTED);
    return PlcErrorCodes::ERROR_TCP_NOT_CONNECTED;
  }
  if (buf == nullptr) {
    log_error("TCP_Channel::read", "Null buffer provided.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Use MSG_DONTWAIT for non-blocking read, as timeout is handled by caller logic or internal
  // select
  ssize_t received_bytes = recv(_socket_fd, buf, n, MSG_DONTWAIT);
  if (received_bytes < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
    log_error("TCP_Channel::read", "Failed to receive data. Error: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_TCP_READ_FAILED);
    return PlcErrorCodes::ERROR_TCP_READ_FAILED;
  }
  if (received_bytes == 0) {  // Connection closed by peer
    PlcErrorCodes rs;
    log_error("TCP_Channel::read", "Peer closed connection.",
              PlcErrorCodes::ERROR_TCP_CONNECTION_FAILED);
    rs = disconnect();  // Automatically disconnect
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("TCP_Channel::read", "Failed to disconnect after peer closed connection.", rs);
      return rs;
    }
    return PlcErrorCodes::ERROR_TCP_READ_FAILED;
  }
  bytes_read = (received_bytes > 0)
                   ? received_bytes
                   : 0;  // If EWOULDBLOCK/EAGAIN, received_bytes is -1, treat as 0 bytes read
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes TCP_Channel::getSocketFD(int &socket_fd) {
  std::lock_guard<std::mutex> lock(_mutex);
  socket_fd = this->_socket_fd;
  return PlcErrorCodes::PLC_SUCCESS;
}
