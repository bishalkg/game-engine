#pragma once

#include "net_common.h"
#include "net_message.h"
#include "net_ts_queue.h"


namespace net
{

  template<typename T>
  class connection : public std::enable_shared_from_this<connection<T>> // this will be a shared pointer rather than raw
  {
    public:

      enum class owner
      {
        server,
        client
      };

      connection(owner parent, asio::io_context& asioCtx, asio::ip::tcp::socket socket, tsqueue<owned_message<T>>& qIn)
      : m_asioContext(asioCtx), m_socket(std::move(socket)), m_qMessagesIn(qIn)
      {
        m_nOwnerType = parent;
      }

      virtual ~connection()
      {}

      uint32_t GetID() const
      {
        return id;
      }

    public:
      void ConnectToClient(uint32_t uid = 0)
      {
        if (m_nOwnerType == owner::server)
        {
          if (IsConnected())
          {
            id = uid;
          }
        }

      }

      bool ConnectToServer();
      bool Disconnect();
      bool IsConnected() const // "this" treated as const, nonmutable cant be modified
      {
        return m_socket.is_open();
      }

      bool Send(const message<T>& msg);


    protected: // class and derived class (unlike private), and friends can access.

      // each connection has unique socket
      asio::ip::tcp::socket m_socket;

      // shared context across connection instances
      asio::io_context& m_asioContext;

      // connection holds queue of msg to be sent out
      tsqueue<message<T>> m_qMessagesOut;

      // holds all msg recieved from remote.
      // is a reference as owner of this conn must provide the queue
      tsqueue<owned_message>& m_qMessagesIn;

      owner m_nOwnerType = owner::server;
      uint32_t id = 0;

  };




}