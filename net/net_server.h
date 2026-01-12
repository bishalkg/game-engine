#pragma once
#include "net_common.h"
#include "net_message.h"
#include "net_ts_queue.h"
#include "net_connection.h"

namespace net
{

  template<typename T>
  class server_interface
  {
    public:
      server_interface(uint16_t port)
      : m_asioAccepter(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
      {

      }

      virtual ~server_interface() {
        Stop();
      }

      bool Start() {
        try
        {
          WaitForClientConnection(); // give context work first so it doesn't close on startup

          m_threadContext = std::thread([this]() { m_asioContext.run(); });

        }
        catch (std::exception& e)
        {
          std::cerr << "Server Exception " << e.what() << "\n";
          return false;
        }

        std::cout << "Server Started!\n";
        return true;
      }

      bool Stop()
      {
        m_asioContext.stop();

        if (m_threadContext.joinable())
        {
          m_threadContext.join();
        }

        std::cout << "Server Stopped!\n"

      }

      void WaitForClientConnection()
      {
        m_asioAccepter.async_accept(
          [this](std::error_code ec, asio::ip::tcp::socket socket)
          {
            if (!ec)
            {
              std::cout << "Server New Connection: " << socket.remote_endpoint() << "\n";

              std::shared_ptr<connection<T>> newconn = std::make_shared<connection<T>>(
                connection<T>::owner::server,
                m_asioContext, std::move(socket), m_qMessagesIn);

              if (OnClientConnect(newconn))
              {

                // add to container of conns
                m_deqConns.push_back(std::move(newconn));

                m_deqConns.back()->ConnectToClient(nIDCounter++);

                std::cout << "[ ConnID: " << m_deqConns.back()->GetID() << "] Connection Approved\n";

              }
              else
              {
                std::cout << "Server Denied Connection: " << socket.remote_endpoint() << "\n";
              }
            }
            else
            {
              std::cout << "Server New Connection Error" << ec.message() << "\n";
            }

            // return to waiting for another conn
            WaitForClientConnection();
          });
      }

      void MessageClient(std::shared_ptr<connection<T>> client, const message<T>& msg)
      {
        if (client && client->IsConnected())
        {
          client->Send(msg);
        }
        else
        {
          OnClientDisconnect(client); // allow user to handle
          client.reset();
          m_deqConns.erase(
            std::remove(m_deqConns.begin(), m_deqConns.end(), client),
            m_deqConns.end()
          );


        }


      }

      void BroadcastToClients(const message<T>& msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
      {
        bool shouldErase = false;
        for (auto& client : m_deqConns)
        {
          if (client && client->IsConnected())
          {
            if (client != pIgnoreClient)
            {
              client->Send(msg);
            }
          }
          else
          {
            OnClientDisconnect();
            client.reset();
            // can't do erase here or could break the for loop
            shouldErase = true;
          }
        }

        if (shouldErase)
        {
          m_deqConns.erase(
            std::remove(m_deqConns.begin(), m_deqConns.end(), nullptr),
            m_deqConns.end()
          );
        }

      }

      void Update(size_t nMaxMessages = -1) // setting unsigned int to -1 sets it to max number
      {
        size_t nMessageCount = 0;

        // limit max number of messages being read on single Update call
        while (nMessageCount < nMaxMessages && !m_qMessagesIn.empty())
        {
          auto msg = m_qMessagesIn.pop_front();

          OnMessage(msg.remote, msg.msg);

          nMessageCount++
        }

      }

      protected:
        virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
        {

          return false;
        }

        virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client)
        {

        }

        virtual void OnMessage(std::shared_ptr<connection<T>> client, message<T>& msg)
        {

        }

      protected:
        tsqueue<owned_message<T>> m_qMessagesIn;

        // container of all valid conns
        std::deque<std::shared_ptr<connection<T>>> m_deqConns;

        asio::io_context m_asioContext;
        std::thread m_threadContext;

        // socket of asio server is abstracted, accepter listens on socket for connections
        asio::ip::tcp::acceptor m_asioAccepter;

        // identify clients via ID
        uint32_t nIDCounter = 10000;


  };


}