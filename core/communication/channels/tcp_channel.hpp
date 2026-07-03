/**
 * @file tcp_channel.hpp
 * @author Diego Arcos Sapena
 * @brief TCP/IP communication channel (header)
 * @version 1.0.0
 * @date 2024/07/25
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */
#pragma once

#include <netinet/in.h>  // For sockaddr_in
#include <sys/socket.h>  // For socket operations

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "../../common/errors.hpp"
#include "Ichannel.hpp"

using TCPChannelPtr = std::shared_ptr<class TCP_Channel>;

/**
 * @class TCP_Channel
 * @brief Implements a TCP/IP communication channel.
 *
 * This class provides methods to connect, disconnect, write, and read data over a TCP/IP socket.
 */
class TCP_Channel final : public Channel {
  public:
  /**
   * @brief Constructs a new TCP_Channel object.
   * @param[in] ip_address The IP address of the server to connect to.
   * @param[in] port The port number to connect to.
   */
  TCP_Channel(const std::string &ip_address, int port);
  /**
   * @brief Destroys the TCP_Channel object, closing the socket if connected.
   */
  ~TCP_Channel();

  /**
   * @brief Establishes a connection to the TCP server.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on
   */
  PlcErrorCodes connect() override;
  /**
   * @brief Disconnects from the TCP server.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes disconnect() override;
  /**
   * @brief Writes a block of bytes to the TCP channel.
   * @param[in] buf Pointer to the data buffer to write.
   * @param[in] n Number of bytes to write.
   * @param[out] bytes_written The number of bytes that were actually written.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes write(const void *buf, size_t n, ssize_t &bytes_written) override;
  /**
   * @brief Reads a block of bytes from the TCP channel.
   * @param[out] buf Pointer to the buffer where read data will be stored.
   * @param[in] n Number of bytes to read.
   * @param[out] bytes_read The number of bytes that were actually read.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes read(void *buf, size_t n, ssize_t &bytes_read) override;

  /**
   * @brief Gets the underlying socket file descriptor.
   * @param[out] socket_fd Reference to an integer where the descriptor will be stored.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes getSocketFD(int &socket_fd);

  private:
  /**
   * @brief The IP address of the module to connect to
   */
  std::string _ip_address;
  /**
   * @brief The port number to connect to
   */
  int _port;
  /**
   * @brief The socket file descriptor for the TCP connection
   */
  int _socket_fd;
  /**
   * @brief Flag indicating whether the channel is connected
   */
  std::atomic<bool> _is_connected;
  /**
   * @brief Mutex for thread-safe access to the socket
   */
  std::mutex _mutex;
};