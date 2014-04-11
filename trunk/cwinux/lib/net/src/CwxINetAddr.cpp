#include "CwxINetAddr.h"
#include <stdlib.h>
CWINUX_BEGIN_NAMESPACE
  /// Default constructor.
  CwxINetAddr::CwxINetAddr(void)
  :CwxAddr(AF_INET, sizeof(inet_addr_.in4_))
{
}
/// Copy constructor.
inline CwxINetAddr::CwxINetAddr (const CwxINetAddr& sa):CwxAddr(sa.getType(), sa.getSize())
{
  this->reset();
  this->set(sa);
}
/// Creates an CwxINetAddr from a sockaddr_in structure.
CwxINetAddr::CwxINetAddr (const sockaddr_in *addr, int len, CWX_INT32 iFamily)
  :CwxAddr(iFamily, sizeof(inet_addr_))
{
  this->reset();
  this->set(addr, len);
}

/// Creates an CwxINetAddr from a @a unPort and the remote
/// @a szHost. The port number is assumed to be in host byte order.
/// To set a port already in network byte order, please @see set().
/// Use iFamily to select IPv6 (AF_INET6) vs. IPv4 (AF_INET).
CwxINetAddr::CwxINetAddr(CWX_UINT16 unPort, char const* szHost, CWX_INT32 iFamily)
  :CwxAddr(iFamily, sizeof(inet_addr_))
{
  memset (&this->inet_addr_, 0, sizeof (this->inet_addr_));
  this->set(unPort, szHost, iFamily);
}

CwxINetAddr::CwxINetAddr(CWX_UINT16 unPort, CWX_INT32 iFamily)
  :CwxAddr(iFamily, sizeof(inet_addr_))
{
  this->reset();
  this->set (unPort, iFamily);

}

/// Uses <getservbyname> to create an CwxINetAddr from a
/// <szPortName>, the remote @a szHostName, and the @a protocol.
CwxINetAddr::CwxINetAddr(char const* szPortName, char const* szHostName, char const* protocol)
  :CwxAddr(AF_INET, sizeof(inet_addr_))
{
  this->reset ();
  this->set(szPortName, szHostName, protocol);
}
/// Default dtor.
CwxINetAddr::~CwxINetAddr (void)
{
}

int CwxINetAddr::set(CWX_UINT16 unPort, char const* szHost, CWX_INT32 iFamily)
{
  CWX_ASSERT(szHost);
  memset (&this->inet_addr_, 0, sizeof this->inet_addr_);
  struct addrinfo hints;
  struct addrinfo *res = 0;
  int error = 0;
  memset (&hints, 0, sizeof (hints));
  if (iFamily == AF_INET6) {
    hints.ai_family = AF_INET6;
    error = ::getaddrinfo (szHost, 0, &hints, &res);
    if (error) {
      if (res) ::freeaddrinfo(res);
      errno = error;
      return -1;
    }
  } else {
    hints.ai_family = AF_INET;
    error = ::getaddrinfo (szHost, 0, &hints, &res);
    if (error) {
      if (res) ::freeaddrinfo(res);
      errno = error;
      return -1;
    }
  }
  this->setType(res->ai_family);
  this->setAddr(res->ai_addr, res->ai_addrlen);
  this->setPort(unPort);
  ::freeaddrinfo (res);
  return 0;
}

int CwxINetAddr::set (CWX_UINT16 unPort, CWX_INT32 iFamily) {

  if (iFamily == AF_INET) {
    this->baseSet(AF_INET, sizeof (this->inet_addr_.in4_));
    this->inet_addr_.in4_.sin_family = AF_INET;
    int ip4 = INADDR_ANY;
    memcpy (&this->inet_addr_.in4_.sin_addr, &ip4, sizeof(ip4));
    setPort(unPort);
    return 0;
  } else {
    this->baseSet (AF_INET6, sizeof (this->inet_addr_.in6_));
    this->inet_addr_.in6_.sin6_family = AF_INET6;
    in6_addr const ip6 = in6addr_any;
    memcpy (&this->inet_addr_.in6_.sin6_addr, &ip6, sizeof (ip6));
    setPort(unPort);
    return 0;
  }
  errno = EAFNOSUPPORT;
  return -1;
}

int CwxINetAddr::set (char const* szPortName, char const* szHostName, char const* protocol)
{
  int port_number = CwxSocket::getSvrPort(szPortName, protocol);
  if (port_number == -1) {
    errno = ENOTSUP;
    return -1;
  }
  int address_family = AF_INET;
  if (strcmp (protocol, "tcp6") == 0)
    address_family = AF_INET6;
  return this->set(static_cast<u_short> (port_number), szHostName, address_family);
}

int CwxINetAddr::set (char const* szPortName, char const* protocol)
{
  int port_number = CwxSocket::getSvrPort(szPortName, protocol);
  if (port_number == -1){
    errno = ENOTSUP;
    return -1;
  }
  int address_family = AF_INET;
  if (strcmp (protocol, "tcp6") == 0)
    address_family = AF_INET6;
  return this->set(static_cast<u_short> (port_number), address_family);
}



/// Return a pointer to the underlying network address.
void * CwxINetAddr::getAddr (void) const
{
  return (void*)&this->inet_addr_;
}

int CwxINetAddr::setAddr (void* addr, CWX_INT32 ) {
  struct sockaddr_in *getfamily = static_cast<struct sockaddr_in *> (addr);
  if (getfamily->sin_family == AF_INET) {
    this->setPort(getfamily->sin_port);
    CWX_UINT32 ip4 = *reinterpret_cast<const CWX_UINT32 *> (&getfamily->sin_addr);
    this->baseSet(AF_INET, sizeof (this->inet_addr_.in4_));
    this->inet_addr_.in4_.sin_family = AF_INET;
    memcpy (&this->inet_addr_.in4_.sin_addr, &ip4, sizeof(ip4));
    return 0;
  } else if (getfamily->sin_family == AF_INET6) {
    struct sockaddr_in6 *in6 = static_cast<struct sockaddr_in6*> (addr);
    this->setPort(in6->sin6_port);
    // We protect ourselves up above so IPv6 must be possible here.
    this->baseSet (AF_INET6, sizeof (this->inet_addr_.in6_));
    this->inet_addr_.in6_.sin6_family = AF_INET6;
    memcpy (&this->inet_addr_.in6_.sin6_addr, &in6->sin6_addr, sizeof (in6->sin6_addr));
    this->inet_addr_.in6_.sin6_scope_id = in6->sin6_scope_id;
    return 0;
  }
  // Here with an unrecognized length.
  errno = EAFNOSUPPORT;
  return -1;
}

CWINUX_END_NAMESPACE

