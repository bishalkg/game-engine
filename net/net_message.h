#pragma once
#include "net_common.h"

namespace net
{

  template <typename T>
  struct message_header
  {
    T id{};
    uint32_t bodySize = 0;
  };

  template <typename T>
  struct message
  {
    message_header<T> header{};
    std::vector<uint8_t> body;

    size_t size() const // TODO rename to bodySize
    {
      return sizeof(message_header<T>) + body.size();
    }

    friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
    {
      os << "ID:" << int(msg.header.id) << "Size:" << msg.header.bodySize;
      return os;
    }

    // push any data into message buffer
    template<typename DataType>
    friend message<T>& operator << (message<T>& msg, const DataType& data)
    {
      static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to push into vector");

      size_t curr_size = msg.body.size();

      msg.body.resize(curr_size + sizeof(DataType));

      // incr pointer to end of vector and then append data
      std::memcpy(msg.body.data() + curr_size, &data, sizeof(DataType));

      msg.header.bodySize = msg.size();

      // return ref to msg
      return msg;
    }

    template<typename DataType>
    friend message<T>& operator >> (message<T>& msg, DataType& data)
    {

      static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to pull from vector");

      // get location at end of body that we want to chip off
      size_t idx = msg.body.size() - sizeof(DataType);

      // copy that data onto data (needs ptr; address of ref == address of underlying data)
      std::memcpy(&data, msg.body.data() + idx, sizeof(DataType));

      // resize since we copied that data
      msg.body.resize(idx);

      msg.header.bodySize = msg.size();

      return msg;
    }



  };


  template <typename T>
  class connection;

  template <typename T>
  struct owned_message
  {
    std::shared_ptr<connection<T>> remote = nullptr; // tag each with connection on server; client only has one so nullptr
    message<T> msg;

    friend std::ostream& operator<<(std::ostream& os, const owned_message<T>& msg)
    {
      os << msg.msg;
      return os;
    }
  };

}