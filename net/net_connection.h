#pragma once

#include "net_common.h"
#include "net_message.h"
#include "net_ts_queue.h"


namespace net
{
  // foreward declare server interface
  template<typename T>
  class server_interface;

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

        if (m_nOwnerType == owner::server) {
          // server will send data to client, client will have to perform the encryption and send it back
          // to the server. server checks if the encrypted number is the same to validate connection.
          m_handShakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

          m_handShakeCheck = encrypt(m_handShakeOut);
        } else {
          // client doesnt need to define anything in constructor. must perform encrypt operation on client connect (ConnectToServer)
          m_handShakeIn = 0;
          m_handShakeOut = 0;
        }

      }

      virtual ~connection()
      {}

      uint32_t GetID() const
      {
        return m_id;
      }

    public:
      void ConnectToClient(net::server_interface<T>* server, uint32_t uid = 0) {
        if (m_nOwnerType == owner::server) {
          if (IsConnected()) {
            m_id = uid;
            // AsyncReadHeader();
            // on initial connect write the raw data to the client
            AsyncWriteValidation();
            // and then wait for client to respond with the encrypted data and attempt to validate
            AsyncReadValidation(server);
          }
        }
      }

      void ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints) {
        if (m_nOwnerType == owner::client) {

          asio::async_connect(
            m_socket,
            endpoints,
            [this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
            {
              if (!ec) {
                // ConnectToClient the server writes the unencrypted val to the client and the client reads it here
                AsyncReadValidation();
                // AsyncReadHeader();
              }
            });
        }
      }

      void Disconnect() {
        asio::post(m_asioContext, [this]() { m_socket.close(); });
      }

      bool IsConnected() const // "this" treated as const, nonmutable cant be modified
      {
        return m_socket.is_open();
      }

      // Send puts the msg into the outbound msg queue
      void Send(const message<T>& msg) {

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

      // AsyncWriteValidation has the server write the unencrypted handshake val to the client for validation. For the client, this is called in AsyncReadValidation after writing the encrypted val back to the server and so client waits at AsyncReadHeader for server writes
      void AsyncWriteValidation() {
        asio::async_write(m_socket, asio::buffer(&m_handShakeOut, sizeof(uint64_t)),
        [this](std::error_code ec, std::size_t length) {
          if (!ec) {
            // client wrote to socket the handShakeOut so sit and wait for response
            if (m_nOwnerType == owner::client) {
              AsyncReadHeader();
            }
          } else {
            m_socket.close();
          }
        });
      }

      void AsyncReadValidation(net::server_interface<T>* server = nullptr) {
        asio::async_read(m_socket, asio::buffer(&m_handShakeIn, sizeof(uint64_t)),
        [this, server](std::error_code ec, std::size_t length) {
          if (!ec) {
            if (m_nOwnerType == owner::server) {
              // server does the validation here
              if (m_handShakeIn == m_handShakeCheck) {

                std::cout << "Client Validated" << std::endl;

                server->OnClientValidated(this->shared_from_this());

                // can sit waiting for data now
                AsyncReadHeader();
              } else {
                  std::cout << "Client Disconnected (Incorrect Encryption)" << std::endl;
                  m_socket.close();
              }
            } else {
              // client does encryption and sends back
              m_handShakeOut = encrypt(m_handShakeIn);
              AsyncWriteValidation();
            }

          } else {
            std::cout << "Client Disconnected (Read Validation Failed)" << std::endl;
            m_socket.close();
          }
        });
      }

      void AsyncReadHeader() {
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
              std::cout << "[" << m_id << "] Read Header Failed.\n";
              m_socket.close();
            }
        });
      }

      void AsyncReadBody() {
        // asio:buffer(ptr_to_data, size_of_data) -> reads size_of_data from socket and writes it to msg body vector, where ptr_to_data points to first idx of the body vector
        asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
          [this](std::error_code ec, std::size_t length)
          {
            if (!ec) {
              AddToIncomingMessageQueue();
            } else {
              std::cout << "[" << m_id << "] Read Body Failed.\n";
              m_socket.close();
            }
        });
      }

      void AsyncWriteHeader() {
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
              std::cout << "[" << m_id << "] Write Header Failed.\n";
              m_socket.close();
            }
          });
      }

      void AsyncWriteBody()
      {
        asio::async_write(m_socket, asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front().body.size()),
          [this](std::error_code ec, std::size_t length)
          {
            if (!ec) {
              m_qMessagesOut.pop_front(); // pop msg off queue once written to socket buffer

              if (!m_qMessagesOut.empty()) {
                AsyncWriteHeader(); // continue writing to socket if we have more messages
              }

            } else {
              std::cout << "[" << m_id << "] Write Body Failed.\n";
              m_socket.close();
            }
          });

      }

      void AddToIncomingMessageQueue() {
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

      uint64_t encrypt(uint64_t input) {
        uint64_t out = input ^ 0xFEEDB066FEEDB066;
        out = (out & 0xF0F0F0F0F0F0F0F0) >> 4 | (out & 0xF0F0F0F0F0F0F0F0) << 4;
        return out ^ 0xFEEDB06612345678;
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
      tsqueue<owned_message<T>>& m_qMessagesIn;
      message<T> m_msgTemporaryIn;

      owner m_nOwnerType = owner::server;
      uint32_t m_id = 0;

      // encryption validation
      uint64_t m_handShakeOut = 0; // sent
      uint64_t m_handShakeIn = 0; // recieved
      uint64_t m_handShakeCheck = 0; // check by server to do comparison
  };




}