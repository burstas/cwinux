/**
 * Autogenerated by Thrift Compiler (0.8.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef echo_TYPES_H
#define echo_TYPES_H

#include <Thrift.h>
#include <TApplicationException.h>
#include <protocol/TProtocol.h>
#include <transport/TTransport.h>



namespace echo_thrift {

typedef struct _EchoData__isset {
  _EchoData__isset() : data(false) {}
  bool data;
} _EchoData__isset;

class EchoData {
 public:

  static const char* ascii_fingerprint; // = "EFB929595D312AC8F305D5A794CFEDA1";
  static const uint8_t binary_fingerprint[16]; // = {0xEF,0xB9,0x29,0x59,0x5D,0x31,0x2A,0xC8,0xF3,0x05,0xD5,0xA7,0x94,0xCF,0xED,0xA1};

  EchoData() : data("") {
  }

  virtual ~EchoData() throw() {}

  std::string data;

  _EchoData__isset __isset;

  void __set_data(const std::string& val) {
    data = val;
  }

  bool operator == (const EchoData & rhs) const
  {
    if (!(data == rhs.data))
      return false;
    return true;
  }
  bool operator != (const EchoData &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const EchoData & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

} // namespace

#endif
