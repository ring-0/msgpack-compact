#pragma once 

#include <limits>
#include <type_traits>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <string.h>
#include <typeinfo>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

namespace mpcompact {

namespace detail {

/******************************************************
 * Fixed length types
 ******************************************************/

// ! Integers
static const uint8_t MP_INT8   = 0xd0;
static const uint8_t MP_INT16  = 0xd1;
static const uint8_t MP_INT32  = 0xd2;
static const uint8_t MP_INT64  = 0xd3;
static const uint8_t MP_UINT8  = 0xcc;
static const uint8_t MP_UINT16 = 0xcd;
static const uint8_t MP_UINT32 = 0xce;
static const uint8_t MP_UINT64 = 0xcf;
static const uint8_t MP_FIXNUM = 0x00; //!< Last 7 bits is value
static const uint8_t MP_NEGATIVE_FIXNUM = 0xe0; //!< Last 5 bits is value

//! nil
static const uint8_t MP_NIL = 0xc0;

//! boolean
static const uint8_t MP_FALSE = 0xc2;
static const uint8_t MP_TRUE  = 0xc3;

//! Floating point
static const uint8_t MP_FLOAT  = 0xca;
static const uint8_t MP_DOUBLE = 0xcb;

/*****************************************************
 * Variable length types
 *****************************************************/

//! String
static const uint8_t MP_STR8   = 0xd9;
static const uint8_t MP_STR16  = 0xda;
static const uint8_t MP_STR32  = 0xdb;
static const uint8_t MP_FIXSTR = 0xa0; //!< Last 5 bits is size

//! Binary
static const uint8_t MP_BIN8  = 0xc4;
static const uint8_t MP_BIN16 = 0xc5;
static const uint8_t MP_BIN32 = 0xc6;


/*****************************************************
 * Container types
 *****************************************************/

//! Arrays
static const uint8_t MP_ARRAY16  = 0xdc;
static const uint8_t MP_ARRAY32  = 0xdd;
static const uint8_t MP_FIXARRAY = 0x90; //<! Lst 4 bits is size

//! Maps
static const uint8_t MP_MAP16  = 0xde;
static const uint8_t MP_MAP32  = 0xdf;
static const uint8_t MP_FIXMAP = 0x80; //<! Last 4 bits is size

//! Some helper bitmasks
static const uint32_t MAX_4BIT  = 0xf;
static const uint32_t MAX_5BIT  = 0x1f;
static const uint32_t MAX_7BIT  = 0x7f;
static const uint32_t MAX_8BIT  = 0xff;
static const uint32_t MAX_15BIT = 0x7fff;
static const uint32_t MAX_16BIT = 0xffff;
static const uint32_t MAX_31BIT = 0x7fffffff;
static const uint32_t MAX_32BIT = 0xffffffff;

static const uint8_t TYPE_1BIT  = 0x80;
static const uint8_t TYPE_3BIT  = 0xe0;
static const uint8_t TYPE_4BIT  = 0xf0;
static const uint8_t VALUE_4BIT = 0x0f;
static const uint8_t VALUE_5BIT = 0x1f;
static const uint8_t VALUE_7BIT = 0x7f;

} // end namespace detail

class PackerStatic
{
    char*        base;
    char*        ptr;
    const size_t capacity;
    size_t       remaining;

public:
    PackerStatic(char* p, size_t c) : base(p), ptr(p), capacity(c), remaining(c) {}

    void write(const void* data, size_t length)
    {
        if(remaining < length)
            throw std::runtime_error("No space remaining in buffer");

        memcpy(ptr, data, length);
        remaining -= length;
        ptr += length;
    }

    const char* data() const    { return static_cast<const char*>(base); }
    size_t      size() const    { return capacity - remaining;           }
    void        reset()         { remaining = capacity; ptr = base;      }
};


class PackerDynamic
{
    std::vector<char>   dataVec;

public:
    PackerDynamic() : dataVec() {}

    const char* data() const { return dataVec.data(); }
    size_t      size() const { return dataVec.size(); }
    void        reset()      { dataVec.resize(0);     }

    void write(const void* data, size_t length)
    {
        const char* ptr = static_cast<const char*>(data);
        dataVec.insert(dataVec.end(), ptr, ptr+length);
    }
};


class Packer
{
    PackerStatic    staticPacker;
    PackerDynamic   dynamicPacker;

    enum { STATIC, DYNAMIC } type;

private:
    Packer& write(const void* data, size_t length)
    {
        if(type == DYNAMIC)
            dynamicPacker.write(data, length);
        else
            staticPacker.write(data, length);

        
        return *this;
    }

    template<typename T>
    Packer& write(T value)
    {
        return(write(&value, sizeof(T)));
    }


    template<typename T>
    Packer& write(uint8_t type, T value)
    {
        struct {
            uint8_t type;
            T       value;
        } __attribute__ (( packed )) buf;

        buf.type = type;
        buf.value = value;

        return(write(&buf, sizeof(buf)));
    }


    template<typename T>
    Packer& pack_integral(T value) 
    {
        if(value >= 0)
        {
            if(value <= detail::MAX_7BIT)
            {
                write<uint8_t>(static_cast<uint8_t>(value) | detail::MP_FIXNUM);
            }
            else if(value <= detail::MAX_8BIT)
            {
                write<uint8_t>(detail::MP_UINT8, value);
            }
            else if(value <= detail::MAX_16BIT)
            {
                write<uint16_t>(detail::MP_UINT16, value);
            }
            else if(value <= detail::MAX_32BIT)
            {
                write<uint32_t>(detail::MP_UINT32, value);
            }
            else
            {
                write<uint64_t>(detail::MP_UINT64, value);
            }
        } else {
            if(value >= -(detail::MAX_5BIT + 1))
            {
                write<int8_t>(static_cast<int8_t>(value) | detail::MP_NEGATIVE_FIXNUM);
            }
            else if(value >= -(detail::MAX_7BIT + 1))
            {
                write<int8_t>(detail::MP_INT8, value);
            }
            else if(value >= -(detail::MAX_15BIT + 1))
            {
                write<int16_t>(detail::MP_INT16, value);
            }
            else if(value >= -(int64_t(detail::MAX_31BIT) + 1))
            {
                write<int32_t>(detail::MP_INT32, value);
            }
            else
            {
                write<int64_t>(detail::MP_INT64, value);
            }
        }

        return *this;
    }


    Packer& pack_boolean(bool value)
    {
        if(value)
        {
            write(detail::MP_TRUE);
        }
        else
        {
            write(detail::MP_FALSE);
        }

        return *this;
    }


    template<typename T>
    Packer& pack_floating_point(T value)
    {
        if(std::is_same<float,T>::value)
        {
            write<float>(detail::MP_FLOAT, value);
        }
        else
        {
            write<double>(detail::MP_DOUBLE, value);
        }

        return *this;
    }


    Packer& pack_string(const char* buffer, size_t length) 
    {
        if(length == 0)
        {
            write(detail::MP_NIL);
            return *this;
        }
        else if(length <= detail::MAX_5BIT)
        {
            write<uint8_t>(static_cast<uint8_t>(length) | detail::MP_FIXSTR);
        }
        else if(length <= detail::MAX_8BIT)
        {
            write<uint8_t>(detail::MP_STR8, length);
        }
        else if(length <= detail::MAX_16BIT)
        {
            write<uint16_t>(detail::MP_STR16, length);
        }
        else if(length <= detail::MAX_32BIT)
        {
            write<uint32_t>(detail::MP_STR32, length);
        }
        else
        {
            throw std::runtime_error("std::string size overflow");
        }

        write(buffer, length);
    
        return *this;
    }


    Packer& pack_binary(const void* buffer, size_t length)
    {
        if(length <= detail::MAX_8BIT)
        {
            write<uint8_t>(detail::MP_BIN8, length);
        }
        else if(length <= detail::MAX_16BIT)
        {
            write<uint16_t>(detail::MP_BIN16, length);
        }
        else if(length <= detail::MAX_32BIT)
        {
            write<uint32_t>(detail::MP_BIN32, length);
        }
        else
        {
            throw std::runtime_error("binary size overflow");
        }

        write(buffer, length);

        return *this;
    }


    template<typename T>
    Packer& pack_array(const T* data, size_t size) 
    {
        if(size <= detail::MAX_4BIT)
        {
            write<uint8_t>(static_cast<uint8_t>(size) | detail::MP_FIXARRAY);
        }
        else if(size <=  detail::MAX_16BIT)
        {
            write<uint16_t>(detail::MP_ARRAY16, size);
        }
        else if(size <= detail::MAX_32BIT)
        {
            write<uint32_t>(detail::MP_ARRAY32, size);
        }
        else
        {
            throw std::runtime_error("Array size overflow");
        }

        for(int i=0; i<size; i++)
            pack(data[i]);

        return *this;
    }

    Packer& pack_array(const std::vector<bool>& ref)
    {
        size_t size = ref.size();
        if(size <= detail::MAX_4BIT)
        {
            write<uint8_t>(static_cast<uint8_t>(size) | detail::MP_FIXARRAY);
        }
        else if(size <=  detail::MAX_16BIT)
        {
            write<uint16_t>(detail::MP_ARRAY16, size);
        }
        else if(size <= detail::MAX_32BIT)
        {
            write<uint32_t>(detail::MP_ARRAY32, size);
        }
        else
        {
            throw std::runtime_error("Array size overflow");
        }

        for(size_t i=0; i<size; i++)
            pack(ref.at(i));

        return *this;
    }

    template<typename K, typename V>
    Packer& pack_map(const std::map<K,V>& ref)
    {
        size_t size = ref.size();
        if(size <= detail::MAX_4BIT)
        {
            write<uint8_t>(static_cast<uint8_t>(size) | detail::MP_FIXMAP);
        }
        else if(size <=  detail::MAX_16BIT)
        {
            write<uint16_t>(detail::MP_MAP16, size);
        }
        else if(size <= detail::MAX_32BIT)
        {
            write<uint32_t>(detail::MP_MAP32, size);
        }
        else
        {
            throw std::runtime_error("std::map size overflow");
        }

        for(auto& kv : ref) {
            pack(kv.first);
            pack(kv.second);
        }
    
        return *this;
    }

public:
    Packer(char* p, size_t r) : staticPacker(p, r), dynamicPacker(), type(STATIC)  {}
    Packer()                  : staticPacker(0, 0), dynamicPacker(), type(DYNAMIC) {}

    void reset()
    {
        if(type == DYNAMIC)
            dynamicPacker.reset();
        else
            staticPacker.reset();
    }

    const char* data() const
    {
        if(type == DYNAMIC)
            return dynamicPacker.data();
        else
            return staticPacker.data();
    }


    size_t size() const
    {
        if(type == DYNAMIC)
            return dynamicPacker.size();
        else
            return staticPacker.size();
    }


    Packer& pack(const char& arg)       { return pack_integral(arg);        }
    Packer& pack(const uint8_t& arg)    { return pack_integral(arg);        }
    Packer& pack(const uint16_t& arg)   { return pack_integral(arg);        }
    Packer& pack(const uint32_t& arg)   { return pack_integral(arg);        }
    Packer& pack(const uint64_t& arg)   { return pack_integral(arg);        }
    Packer& pack(const int8_t& arg)     { return pack_integral(arg);        }
    Packer& pack(const int16_t& arg)    { return pack_integral(arg);        }
    Packer& pack(const int32_t& arg)    { return pack_integral(arg);        }
    Packer& pack(const int64_t& arg)    { return pack_integral(arg);        }
    Packer& pack(const bool& arg)       { return pack_boolean(arg);         }
    Packer& pack(const double& arg)     { return pack_floating_point(arg);  }
    Packer& pack(const float& arg)      { return pack_floating_point(arg);  }

    Packer& pack(const std::string& arg)    { return pack_string(arg.data(), arg.length()); }
    Packer& pack(const char*& arg)          { return pack_string(arg, strlen(arg));         }

    // T arg[]
    template<typename T, std::size_t N>
    typename std::enable_if<sizeof(T) == 1, Packer&>::type 
    pack(T (&arg)[N])                       { return pack_binary(arg, N);                   }

    template<typename T, std::size_t N>
    typename std::enable_if<(sizeof(T) > 1), Packer&>::type 
    pack(T (&arg)[N])                       { return pack_array<T>(arg, N);                 }

    // std::vector<T>
    template<typename T>
    typename std::enable_if<sizeof(T) == 1, Packer&>::type
    pack(const std::vector<T>& arg)         { return pack_binary(arg.data(), arg.size());   }

    template<typename T>
    typename std::enable_if<(sizeof(T) > 1), Packer&>::type 
    pack(const std::vector<T>& arg)         { return pack_array(arg.data(), arg.size());    }

    Packer& pack(const std::vector<bool>& arg)  { return pack_array(arg);                   }

    template<typename K, typename V>
    Packer& pack(const std::map<K,V>& arg)      { return pack_map(arg);                     }
};
    

class Unpacker
{
    const char* readBufferPtr;
    size_t      remaining;

private:
    Unpacker& consume(size_t length)
    {
        if(remaining < length)
            throw std::runtime_error("No bytes remaining in buffer");
    
        readBufferPtr += length;
        remaining -= length;

        return(*this);
    }

    void read(void* dst, size_t length)
    {
        if(remaining < length)
            throw std::runtime_error("No bytes remaining in buffer");

        memcpy(dst, readBufferPtr, length);

        readBufferPtr += length;
        remaining -= length;
    }

    template<typename T>
    T read()
    {

        if(remaining < sizeof(T))
            throw std::runtime_error("No bytes remaining in buffer");

        T value = *reinterpret_cast<const T*>(readBufferPtr);

        readBufferPtr += sizeof(T);
        remaining -= sizeof(T);

        return(value);
    }


    template<typename T>
    T peek()
    {
        if(remaining < sizeof(T))
            throw std::runtime_error("No bytes remaining in buffer");

        T value = *reinterpret_cast<const T*>(readBufferPtr);

        return(value);
    }


    template<typename T>
    Unpacker& unpack_integral(T& ref)
    {
        uint8_t head = read<uint8_t>();

        if((head & detail::TYPE_1BIT) == detail::MP_FIXNUM) {
            ref = head & detail::VALUE_7BIT;
            return *this;
        }

        if((head & detail::TYPE_3BIT) == detail::MP_NEGATIVE_FIXNUM) {
            ref = -(head & detail::VALUE_5BIT);
            return *this;
        }

        
        uint64_t uVal = 0;
        int64_t sVal  = 0;
        enum { S, U } type;
        switch(head)
        {
            case detail::MP_UINT8:  uVal = read<uint8_t>();  type = U; break;
            case detail::MP_UINT16: uVal = read<uint16_t>(); type = U; break;
            case detail::MP_UINT32: uVal = read<uint32_t>(); type = U; break;
            case detail::MP_UINT64: uVal = read<uint64_t>(); type = U; break;
            case detail::MP_INT8:   sVal = read<int8_t>();   type = S; break;
            case detail::MP_INT16:  sVal = read<int16_t>();  type = S; break;
            case detail::MP_INT32:  sVal = read<int32_t>();  type = S; break;
            case detail::MP_INT64:  sVal = read<int64_t>();  type = S; break;

            default:    throw std::runtime_error("Invalid type received");
        }

        if(type == U)
        {
            if(uVal > std::numeric_limits<T>::max())
                throw std::overflow_error("Value overflows numeric limit");

            ref = uVal;
        }
        else
        {
            if(sVal > std::numeric_limits<T>::max())
                throw std::overflow_error("Value overflows numeric limit");

            if(sVal < std::numeric_limits<T>::min())
                throw std::underflow_error("Value underflows numeric limit");

            ref = sVal;
        }

        return *this;
    }


    Unpacker& unpack_boolean(bool& ref)
    {
        uint8_t head = read<uint8_t>();
        if(head == detail::MP_TRUE)
        {
            ref = true;
        }
        else if(head == detail::MP_FALSE)
        {
            ref = false;
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        return *this;
    }

    template<typename T>
    Unpacker& unpack_floating_point(T& ref)
    {
        uint8_t head = read<uint8_t>();
        if(head == detail::MP_FLOAT)
        {
            ref = read<float>();
        }
        else if(head == detail::MP_DOUBLE)
        {
            double value = read<double>();
            if(value > std::numeric_limits<T>::max())
                throw std::overflow_error("Value overflows numeric limit");

            if(value < std::numeric_limits<T>::min())
                throw std::underflow_error("Value underflows numeric limit");


            ref = value;
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        return *this;
    }


    Unpacker& unpack_string(std::string& ref)
    {
        uint8_t head = read<uint8_t>();

        size_t length;
        if(head == detail::MP_NIL)
        {
            ref.clear();
            return *this;
        }
        else if((head & detail::TYPE_3BIT) == detail::MP_FIXSTR) 
        {
            length = head & detail::VALUE_5BIT;
        }
        else if(head == detail::MP_STR8)
        {
            length = read<uint8_t>();
        }
        else if(head == detail::MP_STR16)
        {
            length = read<uint16_t>();
        }
        else if(head == detail::MP_STR32)
        {
            length = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        // direct access for performance reasons
        if(remaining < length)
            throw std::runtime_error("No bytes remaining in buffer");

        ref.assign(readBufferPtr, length);
        consume(length);
    
        return *this;
    }


    Unpacker& unpack_c_string(char* ptr, size_t size)
    {
        uint8_t head = read<uint8_t>();

        size_t length;
        if(head == detail::MP_NIL)
        {
            *ptr = 0;
            return *this;
        }
        else if((head & detail::TYPE_3BIT) == detail::MP_FIXSTR) 
        {
            length = head & detail::VALUE_5BIT;
        }
        else if(head == detail::MP_STR8)
        {
            length = read<uint8_t>();
        }
        else if(head == detail::MP_STR16)
        {
            length = read<uint16_t>();
        }
        else if(head == detail::MP_STR32)
        {
            length = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        if(length > size)
            throw std::overflow_error("String buffer overflow");

        memset(ptr+length, 0, size-length); 
        read(ptr, length);

        return *this;
    }


    Unpacker& unpack_binary(void* buffer, size_t size)
    {
        uint8_t head = read<uint8_t>();

        size_t length;
        if(head == detail::MP_BIN8)
        {
            length = read<uint8_t>();
        }
        else if(head == detail::MP_BIN16)
        {
            length = read<uint16_t>();
        }
        else if(head == detail::MP_BIN32)
        {
            length = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        if(length != size)
            throw std::overflow_error("Binary buffer size mismatch");

        read(buffer, length);
    
        return *this;
    }


    template<typename T>
    Unpacker& unpack_binary(std::vector<T>& ref)
    {
        uint8_t head = read<uint8_t>();

        size_t length;
        if(head == detail::MP_BIN8)
        {
            length = read<uint8_t>();
        }
        else if(head == detail::MP_BIN16)
        {
            length = read<uint16_t>();
        }
        else if(head == detail::MP_BIN32)
        {
            length = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        ref.resize(length);

        read(ref.data(), length);
    
        return *this;
    }


    template<typename T>
    Unpacker& unpack_array(T* data, size_t size) 
    {
        uint8_t head = read<uint8_t>();

        size_t elements;
        if((head & detail::TYPE_4BIT) == detail::MP_FIXARRAY)
        {
            elements = (head & detail::VALUE_4BIT);
        }
        else if(head == detail::MP_ARRAY16)
        {
            elements = read<uint16_t>();
        }
        else if(head == detail::MP_ARRAY32)
        {
            elements = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        if(elements != size)
            throw std::runtime_error("Array size mismatch");
        
        for(int i=0; i<size; i++)
            unpack(data[i]);

        return *this;
    }


    template<typename T>
    Unpacker& unpack_array(std::vector<T>& ref)
    {
        uint8_t head = read<uint8_t>();

        size_t elements;
        if((head & detail::TYPE_4BIT) == detail::MP_FIXARRAY)
        {
            elements = (head & detail::VALUE_4BIT);
        }
        else if(head == detail::MP_ARRAY16)
        {
            elements = read<uint16_t>();
        }
        else if(head == detail::MP_ARRAY32)
        {
            elements = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        ref.resize(elements);
        
        for(size_t i=0; i<elements; i++)
            unpack(ref.at(i));

        return *this;
    }

    Unpacker& unpack_array(std::vector<bool>& ref)
    {
        uint8_t head = read<uint8_t>();

        size_t elements;
        if((head & detail::TYPE_4BIT) == detail::MP_FIXARRAY)
        {
            elements = (head & detail::VALUE_4BIT);
        }
        else if(head == detail::MP_ARRAY16)
        {
            elements = read<uint16_t>();
        }
        else if(head == detail::MP_ARRAY32)
        {
            elements = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        ref.resize(elements);

        bool value;
        for(size_t i=0; i<elements; i++) {
            unpack(value);
            ref.at(i) = value;
        }

        return *this;
    }


    template<typename K, typename V>
    Unpacker& unpack_map(std::map<K,V>& ref)
    {
        uint8_t head = read<uint8_t>();

        size_t elements;
        if((head & detail::TYPE_4BIT) == detail::MP_FIXMAP)
        {
            elements = (head & detail::VALUE_4BIT);
        }
        else if(head == detail::MP_MAP16)
        {
            elements = read<uint16_t>();
        }
        else if(head == detail::MP_MAP32)
        {
            elements = read<uint32_t>();
        }
        else
        {
            throw std::runtime_error("Invalid type received");
        }

        K key;
        V value;
        for(size_t i=0; i<elements; i++)
        {
            unpack(key);
            unpack(value);
            ref[key] = value;
        }

        return *this;
    }

    friend void toString(mpcompact::Unpacker&, std::string&, bool);

public:
    Unpacker(const char* p, size_t r)
        : readBufferPtr(p), remaining(r) {}

    size_t size() const { return remaining; }
    void consumeAll()   { remaining = 0;    }


    Unpacker& unpack(char& arg)     { return unpack_integral<char>(arg);        }
    Unpacker& unpack(uint8_t& arg)  { return unpack_integral<uint8_t>(arg);     }
    Unpacker& unpack(uint16_t& arg) { return unpack_integral<uint16_t>(arg);    }
    Unpacker& unpack(uint32_t& arg) { return unpack_integral<uint32_t>(arg);    }
    Unpacker& unpack(uint64_t& arg) { return unpack_integral<uint64_t>(arg);    }
    Unpacker& unpack(int8_t& arg)   { return unpack_integral<int8_t>(arg);      }
    Unpacker& unpack(int16_t& arg)  { return unpack_integral<int16_t>(arg);     }
    Unpacker& unpack(int32_t& arg)  { return unpack_integral<int32_t>(arg);     }
    Unpacker& unpack(int64_t& arg)  { return unpack_integral<int64_t>(arg);     }
    Unpacker& unpack(bool& arg)     { return unpack_boolean(arg);                   }
    Unpacker& unpack(double& arg)   { return unpack_floating_point<double>(arg);    }
    Unpacker& unpack(float& arg)    { return unpack_floating_point<float>(arg);     }

    Unpacker& unpack(std::string& arg)      { return unpack_string(arg);        }
    Unpacker& unpack(char*& arg, size_t sz) { return unpack_c_string(arg, sz);  }

    template<typename T, std::size_t N>
    typename std::enable_if<sizeof(T) == 1, Unpacker&>::type 
    unpack(T (&arg)[N])                     { return unpack_binary(arg, N);     }

    template<typename T, std::size_t N>
    typename std::enable_if<(sizeof(T) > 1), Unpacker&>::type 
    unpack(T (&arg)[N])                     { return unpack_array<T>(arg, N);   }

    template<typename T>
    typename std::enable_if<sizeof(T) == 1, Unpacker&>::type 
    unpack(std::vector<T>& arg)             { return unpack_binary(arg);        }

    template<typename T>
    typename std::enable_if<(sizeof(T) > 1), Unpacker&>::type 
    unpack(std::vector<T>& arg)             { return unpack_array(arg);         }

    Unpacker& unpack(std::vector<bool>& arg)    { return unpack_array(arg);     }

    template<typename K, typename V>
    Unpacker& unpack(std::map<K,V>& arg)        { return unpack_map(arg);       }
};

} // end namespace mpcompact

#pragma GCC diagnostic pop

