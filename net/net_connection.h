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
            AsyncReadHeader();
          }
        }

      }

      bool ConnectToServer();

      bool Disconnect() {
        asio::post(m_asioContext, [this]() { m_socket.close(); });
      }

      bool IsConnected() const // "this" treated as const, nonmutable cant be modified
      {
        return m_socket.is_open();
      }

      // Send puts the msg into the outbound msg queue
      bool Send(const message<T>& msg) {

        asio::post(m_asioContext,
          [this, msg]()
          {
            bool asioCtxAlreadyWriting = !m_qMessagesOut.empty();
            m_qMessagesOut.push_back(msg);
            if (!asioCtxAlreadyWriting) {
              AsyncWriteHeader(); // only add write header workloads to asio ctx that isnt already doing task
            }
          });
      }

    private:
      void AsyncReadHeader()
      {
        asio::async_read(
          m_socket,
          asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
          [this](std::error_code ec, std::size_t length)
          {
            if (!ec) {
              if (m_msgTemporaryIn.header.bodySize > 0) {
                m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.bodySize); // resize the tmp buffer for when body is copied into it
                AsyncReadBody();
              } else {
                // no body, just header
                AddToIncomingMessageQueue();
              }
            } else {
              std::cout << "[" << id << "] Read Header Failed.\n";
              m_socket.close();
            }
        })
      }

      void AsyncReadBody()
      {
        // asio:buffer(ptr_to_data, size_of_data) -> reads size_of_data from socket and writes it to msg body vector, where ptr_to_data points to first idx of the body vector
        asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
          [this](std::error_code ec, std::size_t length)
          {
            if (!ec) {
              AddToIncomingMessageQueue();
            } else {
              std::cout << "[" << id << "] Read Body Failed.\n";
              m_socket.close();
            }
        })
      }

      void AsyncWriteHeader()
      {
        asio::async_write(
          m_socket,
          asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
          [this](std::error_code ec, std::size_t length)
          {
            if (!ec) {
              if (m_qMessagesOut.front().body.size() > 0) {
                AsyncWriteBody();
              } else {
                m_qMessagesOut.pop_front();

                if (!m_qMessagesOut.empty())
                {
                  AsyncWriteHeader();
                }
              }
            } else {
              std::cout << "[" << id << "] Write Header Failed.\n";
              m_socket.close();
            }
          })
      }

      void AsyncWriteBody()
      {
        asio::async_write(m_socket, asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front.body.size()),
          [this](std::error_code ec, std::size_t length)
          {
            if (!ec) {
              m_qMessagesOut.pop_front(); // pop msg off queue once written to socket buffer

              if (!m_qMessagesOut.empty()) {
                AsyncWriteHeader(); // continue writing to socket if we have more messages
              }

            } else {
              std::cout << "[" << id << "] Write Body Failed.\n";
              m_socket.close();
            }
          })

      }

      void AddToIncomingMessageQueue()
      {
        // push into client or server queue
        if (m_nOwnerType == owner::server) {
          // server has many connections so when we push to servers queue, we store ref to the connection
          m_qMessagesIn.push_back({ this->shared_from_this(), m_msgTemporaryIn }); // shared_from_this() gives shared pointer to connection
        } else {
          m_qMessagesIn.push_back({ nullptr, m_msgTemporaryIn });
        }

        // register another async asio task
        AsyncReadHeader();
      }


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
      message<T> m_msgTemporaryIn;

      owner m_nOwnerType = owner::server;
      uint32_t id = 0;

  };




}