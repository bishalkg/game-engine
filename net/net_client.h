#pragma once
#include "net_common.h"
#include "net_message.h"
#include "net_ts_queue.h"
#include "net_connection.h"

namespace net {


  template <typename T>
  class client_interface
  {

    public:

      client_interface() : m_socket(m_context){} // init socket with io context

      virtual ~client_interface() { Disconnect(); } // disconnect if client destroyed

      bool Connect(const std::string& host, const uint16_t port)
      {
        try {

          asio::ip::tcp::resolver resolver(m_context);

          asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

          m_connection = std::make_unique<connection<T>>(
            connection<T>::owner::client,
            m_context,
            asio::ip::tcp::socket(m_context),
            m_qMessagesIn
          );

          m_connection->ConnectToServer(endpoints);

          thrContext = std::thread([this](){ m_context.run(); }); // start new thread with context
        }
        catch (std::exception& e)
        {
          std::cerr << "Client Exception " << e.what() << "\n";
          return false;
        }

        return true;
      }

      void Disconnect() {

        if (IsConnected()) {
          m_connection->Disconnect();
        }

        m_context.stop();

        if (thrContext.joinable()) {
          thrContext.join();
        }

        m_connection.release();
      }

      bool IsConnected() {
        if (m_connection) {
          return m_connection->IsConnected();
        }

        return false;
      }

      void Send(const message<T>& msg) {
        if (IsConnected()) {
          m_connection->Send(msg);
        }
      }

      tsqueue<owned_message<T>>& Incoming() {
        return m_qMessagesIn;
      }

    protected:
      // client owns the asio context
      asio::io_context m_context;
      // client does work in own thread
      std::thread thrContext;
      // socket that is connected to server
      asio::ip::tcp::socket m_socket;
      // client only holds single connection to server
      std::unique_ptr<connection<T>> m_connection;


    private:
      // client owns the queue of messages coming in, referenced in the connection
      tsqueue<owned_message<T>> m_qMessagesIn;

  };



}