/**
 * @file protocol.cpp
 * @author Diego Arcos Sapena
 * @brief Abstract base class for all communication protocols (implementation).
 * @version b-1.0.0
 * @date 2024/07/11
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "Iprotocol.hpp"

/**
 * @brief Constructs the base protocol object.
 *
 * Initializes the protocol handler with a generic communication channel. The
 * channel is passed up from derived classes (e.g., SPI, Modbus RTU).
 *
 * @param channel A shared pointer to an object that implements the Channel interface.
 */
Protocol::Protocol(ChannelPtr channelPtr) : _channelPtr(channelPtr) {}
