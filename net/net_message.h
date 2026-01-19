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
    };



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

  struct ByteWriter {
    std::vector<uint8_t> buff;

    // write bytes reads a pointer to some memory (void type not known now)
    // and casts the underlying type of the value
    // to a byte and returns a pointer to the first byte.
    // n is how many bytes to copy
    void write_bytes(const void* p, size_t n) {
      const uint8_t* d = static_cast<const std::uint8_t*>(p);
      // insert(iterator_pos, input first byte ptr, +1 past last byte)
      // and this copies byte by byte to d
      buff.insert(buff.end(), d, d+n);
    };

    void write_u8(const std::uint8_t v) {
      // take the value and pass a pointer to the value to write_bytes
      write_bytes(&v, sizeof(v));
    }
    void write_u16(const std::uint16_t v) {
      write_bytes(&v, sizeof(v));
    }
    void write_u32(const std::uint32_t v) {
      write_bytes(&v, sizeof(v));
    };
    void write_u64(const std::uint64_t v) {
      write_bytes(&v, sizeof(v));
    };
    void write_float(const float_t v) {
      write_bytes(&v, sizeof(v));
    };
    void write_bool(const bool v) {
      write_bytes(&v, sizeof(v));
    };

    void write_glm_vec2(const glm::vec2& v) {
      write_float(v.x);
      write_float(v.y);
    };

    void write_sdl_frect(const SDL_FRect& v) {
      write_float(v.x);
      write_float(v.y);
      write_float(v.w);
      write_float(v.h);
    };

    template<class EnumType>
    void write_enum(const EnumType& v) {
      // check if its type enum
      static_assert(std::is_enum_v<EnumType>, "write_enum requires an enum type.");
      // if valid enum get size of enum
      using U = std::underlying_type_t<EnumType>;
      U data = static_cast<U>(v);
      size_t d_size = sizeof(v);
      switch (d_size) {
        case 0:
        {
          static_assert("Unsupported enum type");
          break;
        }
        case 1:
        {
          write_u8(static_cast<std::uint8_t>(data));
          break;
        }
        case 2:
        {
          write_u16(static_cast<std::uint16_t>(data));
          break;
        }
        case 4:
        {
          write_u32(static_cast<std::uint32_t>(data));
          break;
        }
        case 8:
        {
          write_u64(static_cast<std::uint64_t>(data));
          break;
        }
      }
      // use size of enum to switch number of bytes to write

    };


  };

  struct ByteReader {
    const std::uint8_t* p = nullptr;
    size_t n;
    size_t i;

    // set pointer to input data onto ByteReader
    ByteReader(const std::vector<std::uint8_t>& b): p(b.data()), n(b.size()), i(0) {}

    // copy into out address the value of size sz from position p+i
    // advance index i forward size of the data that was copied
    void read_bytes(void* out, size_t sz) {
      if (i + sz > n) throw std::runtime_error("buffer underflow");
      std::memcpy(out, p+i, sz);
      i += sz;
    };

    // the input to the ByteWrite will deconstruct the struct and write byte by byte each type
    // but where you define how many bytes each piece of data
    std::uint8_t read_u8() { std::uint8_t v; read_bytes(&v, sizeof(v)); return v; };
    std::uint16_t read_u16() { std::uint16_t v; read_bytes(&v, sizeof(v)); return v; };
    std::uint32_t read_u32() { std::uint32_t v; read_bytes(&v, sizeof(v)); return v; };
    std::uint64_t read_u64() { std::uint64_t v; read_bytes(&v, sizeof(v)); return v; };
    std::float_t read_float() { std::float_t v; read_bytes(&v, sizeof(v)); return v; };
    bool read_bool() { bool v; read_bytes(&v, sizeof(v)); return v; };
    glm::vec2 read_glm_vec2() {
      glm::vec2 vec;
      vec.x = read_float();
      vec.y = read_float();
      return vec;
    };

    // EntityType t = r.read_enum<EntityType>();
    template<class EnumType>
    EnumType read_enum() {
      // check if its type enum
      static_assert(std::is_enum_v<EnumType>, "write_enum requires an enum type.");
      // if valid enum get size of enum
      using U = std::underlying_type_t<EnumType>;
      U data{};
      size_t d_size = sizeof(U);

      switch (d_size) {
        case 0:
        {
          static_assert("0 data enum");
          break;
        }
        case 1:
        {
          data = static_cast<U>(read_u8());
          break;
        }
        case 2:
        {
          data = static_cast<U>(read_u16());
          break;
        }
        case 4:
        {
          data = static_cast<U>(read_u32());
          break;
        }
        case 8:
        {
          data = static_cast<U>(read_u64());
          break;
        }
        default: {
          static_assert("Unsupported enum type");
          break;
        }
      }

      return static_cast<EnumType>(data);
    };

  };

}