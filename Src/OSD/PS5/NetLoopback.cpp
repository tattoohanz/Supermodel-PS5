/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **/

/*
 * NetLoopback.cpp
 *
 * The network board's TCP helpers, inside the process.
 *
 * Sega Rally 2, Daytona 2, Scud Race and the other linked games will not boot
 * past their network test unless the board is attached and connected; the
 * desktop build does that by sending to itself over 127.0.0.1 (PortOut ==
 * PortIn). Here a send to the loopback address goes straight into the
 * receiver's queue for that port: the same framed messages, no sockets.
 * Another address never connects (no linked cabinets on the PS5 yet).
 */

#include "Network/TCPReceive.h"
#include "Network/TCPSend.h"
#include "Network/TCPSendAsync.h"
#include "OSD/Logger.h"
#include "OSD/Thread.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

// One port's message queue. TCPsocket points at it (SDLShim.h declares the
// struct opaque). Never freed: a few exist for the life of the process.
struct _TCPsocket
{
  int port = 0;
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::vector<char>> queue;
  bool listening = false;
  bool connected = false;
};

static std::mutex s_registryMutex;
static std::map<int, _TCPsocket *> s_channels;

static _TCPsocket *GetChannel(int port)
{
  std::lock_guard<std::mutex> lock(s_registryMutex);
  _TCPsocket *&channel = s_channels[port];
  if (!channel)
  {
    channel = new _TCPsocket();
    channel->port = port;
  }
  return channel;
}

static bool IsLoopback(const std::string &ip)
{
  return ip == "localhost" || ip.compare(0, 4, "127.") == 0;
}

static TCPsocket ConnectTo(const std::string &ip, int port)
{
  if (!IsLoopback(ip))
  {
    static bool s_logged = false;
    if (!s_logged)
    {
      s_logged = true;
      ErrorLog("Net board: %s:%d is not this machine; linked cabinets are not supported on the PS5 yet.",
        ip.c_str(), port);
    }
    CThread::Sleep(100);  // the callers retry in a loop
    return nullptr;
  }
  _TCPsocket *channel = GetChannel(port);
  std::lock_guard<std::mutex> lock(channel->mutex);
  if (!channel->listening)
  {
    CThread::Sleep(1);
    return nullptr;
  }
  if (!channel->connected)
    InfoLog("Net board: loopback link on port %d connected.", port);
  channel->connected = true;
  channel->cv.notify_all();
  return channel;
}

static bool SendTo(TCPsocket channel, const void *data, int length)
{
  if (!channel)
    return false;
  std::lock_guard<std::mutex> lock(channel->mutex);
  if (!channel->listening)
    return false;
  const char *bytes = (const char *)data;
  channel->queue.emplace_back(bytes, bytes + (length > 0 ? length : 0));
  channel->cv.notify_all();
  return true;
}

TCPSend::TCPSend(std::string &ip, int port) : m_ip(ip), m_port(port), m_socket(nullptr)
{
}

TCPSend::~TCPSend()
{
}

bool TCPSend::Send(const void *data, int length)
{
  return SendTo(m_socket, data, length);
}

bool TCPSend::Connect()
{
  if (!m_socket)
    m_socket = ConnectTo(m_ip, m_port);
  return Connected();
}

bool TCPSend::Connected()
{
  return m_socket != nullptr;
}

TCPSendAsync::TCPSendAsync(std::string &ip, int port) : m_ip(ip), m_port(port), m_socket(nullptr), m_hasData(false)
{
}

TCPSendAsync::~TCPSendAsync()
{
}

bool TCPSendAsync::Send(const void *data, int length)
{
  return SendTo(m_socket, data, length);
}

bool TCPSendAsync::Connect()
{
  if (!m_socket)
    m_socket = ConnectTo(m_ip, m_port);
  return Connected();
}

bool TCPSendAsync::Connected()
{
  return m_socket != nullptr;
}

void TCPSendAsync::SendThread()
{
}

TCPReceive::TCPReceive(int port) : m_listenSocket(nullptr), m_receiveSocket(nullptr), m_socketSet(nullptr), m_running(false)
{
  _TCPsocket *channel = GetChannel(port);
  std::lock_guard<std::mutex> lock(channel->mutex);
  channel->listening = true;
  channel->connected = false;
  channel->queue.clear();
  m_listenSocket = channel;
  m_running = true;
}

TCPReceive::~TCPReceive()
{
  m_running = false;
  if (m_listenSocket)
  {
    std::lock_guard<std::mutex> lock(m_listenSocket->mutex);
    m_listenSocket->listening = false;
    m_listenSocket->connected = false;
    m_listenSocket->queue.clear();
    m_listenSocket->cv.notify_all();
  }
}

bool TCPReceive::CheckDataAvailable(int timeoutMS)
{
  _TCPsocket *channel = m_listenSocket;
  if (!channel)
    return false;
  std::unique_lock<std::mutex> lock(channel->mutex);
  if (!channel->connected)
    return false;
  auto ready = [channel]() { return !channel->queue.empty() || !channel->listening; };
  if (timeoutMS < 0)
    channel->cv.wait(lock, ready);
  else if (timeoutMS > 0)
    channel->cv.wait_for(lock, std::chrono::milliseconds(timeoutMS), ready);
  return !channel->queue.empty();
}

std::vector<char> &TCPReceive::Receive()
{
  m_recBuffer.clear();
  _TCPsocket *channel = m_listenSocket;
  if (!channel)
    return m_recBuffer;
  std::unique_lock<std::mutex> lock(channel->mutex);
  if (!channel->connected)
    return m_recBuffer;
  // Blocks like a socket read would.
  channel->cv.wait(lock, [channel]() { return !channel->queue.empty() || !channel->listening; });
  if (!channel->queue.empty())
  {
    m_recBuffer.swap(channel->queue.front());
    channel->queue.pop_front();
  }
  return m_recBuffer;
}

bool TCPReceive::Connected()
{
  _TCPsocket *channel = m_listenSocket;
  if (!channel)
    return false;
  std::lock_guard<std::mutex> lock(channel->mutex);
  if (channel->connected)
    m_receiveSocket = channel;
  return channel->connected;
}

void TCPReceive::ListenFunc()
{
}
