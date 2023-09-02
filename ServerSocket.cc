/*
 * Copyright ©2023 Timmy Yang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Summer Quarter 2023 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <sstream>       // for stringstream

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::string;

namespace hw4 {

// Gets the IP address and (optionally) port represented by the given
// sockaddr_storage. If port is null, will not return the port. Writes the ip
// address to ret_addr and the port to port. Returns whether or not the address
// was successfully gotten. Requires the ip to be IPv4 or IPv6.
static bool Get_IP(sockaddr_storage, string * const, uint16_t * const);
// Gets the DNS name associated with the given sockaddr_storage. Returns an
// error code representing whether the name was successfully gotten: 0 if it
// was, an error code from getnameinfo otherwise.
static int Get_DNS(sockaddr_storage, socklen_t, string * const);

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int* const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));

  // Set appropriate flags.
  hints.ai_family = ai_family;      // set to use determined ai_family
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol

  std::stringstream ss;
  ss << port_;

  if (getaddrinfo(nullptr, ss.str().c_str(), &hints, &res) != 0) return false;

  int sock_fd = -1;
  for (struct addrinfo* addr = res; addr != nullptr; addr = addr->ai_next) {
    sock_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (sock_fd == -1) continue;
    if (bind(sock_fd, addr->ai_addr, addr->ai_addrlen) == 0) {
      sock_family_ = addr->ai_family;
      break;
    }
    close(sock_fd);
    sock_fd = -1;
  }
  if (sock_fd == -1) return false;

  freeaddrinfo(res);
  *listen_fd = listen_sock_fd_ = sock_fd;

  if (listen(sock_fd, SOMAXCONN) != 0) {
    close(sock_fd);
    return false;
  }
  return true;
}

bool ServerSocket::Accept(int* const accepted_fd,
                          std::string* const client_addr,
                          uint16_t* const client_port,
                          std::string* const client_dns_name,
                          std::string* const server_addr,
                          std::string* const server_dns_name) const {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  // STEP 2:
  // Accept connections.

  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  int client;
  while ((client = accept(listen_sock_fd_,
                          reinterpret_cast<struct sockaddr*>(&caddr),
                          &caddr_len)) < 0) {
    if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      continue;
    return false;
  }
  *accepted_fd = client;

  // Get the client IP and DNS
  if (!Get_IP(caddr, client_addr, client_port)) return false;
  int dns_success = Get_DNS(caddr, caddr_len, client_dns_name);
  if (dns_success == EAI_NONAME) *client_dns_name = *client_addr;
  else if (dns_success != 0) return false;

  // Do it all over again with the server
  struct sockaddr_storage saddr;
  socklen_t saddr_len = sizeof(saddr);
  if (getsockname(client, reinterpret_cast<sockaddr*>(&saddr),
                  &saddr_len) != 0)
    return false;

  if (!Get_IP(saddr, server_addr, nullptr)) return false;
  dns_success = Get_DNS(saddr, saddr_len, server_dns_name);
  if (dns_success == EAI_NONAME) *server_dns_name = *server_addr;
  else if (dns_success != 0) return false;

  return true;
}

static bool Get_IP(
  sockaddr_storage addr,
  string * const ret_addr,
  uint16_t * const port
) {
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in v4addr = *reinterpret_cast<struct sockaddr_in*>(&addr);

    char ipstring[INET_ADDRSTRLEN];
    if (!inet_ntop(addr.ss_family, &v4addr.sin_addr,
                   ipstring, INET_ADDRSTRLEN))
      return false;

    if (port != nullptr) *port = v4addr.sin_port;
    *ret_addr = string(ipstring, strlen(ipstring));
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6 v6addr = *reinterpret_cast<struct sockaddr_in6*>(&addr);

    char ipstring[INET6_ADDRSTRLEN];
    if (!inet_ntop(addr.ss_family, &v6addr.sin6_addr,
                   ipstring, INET6_ADDRSTRLEN))
      return false;

    if (port != nullptr) *port = v6addr.sin6_port;
    *ret_addr = string(ipstring, strlen(ipstring));
  } else {
    return false;
  }
  return true;
}

static int Get_DNS(sockaddr_storage addr, socklen_t len, string * const dns) {
  int result;
  char dns_name[NI_MAXHOST];
  while ((result = getnameinfo(reinterpret_cast<sockaddr*>(&addr), len,
                               dns_name, NI_MAXHOST, nullptr, 0, 0))
          == EAI_AGAIN) {}
  if (result == 0) *dns = dns_name;
  return result;
}

}  // namespace hw4
