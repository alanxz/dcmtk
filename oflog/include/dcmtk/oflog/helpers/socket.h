// Module:  Log4CPLUS
// File:    socket.h
// Created: 4/2003
// Author:  Tad E. Smith
//
//
// Copyright 2003-2009 Tad E. Smith
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/** @file */

#ifndef DCMTK_LOG4CPLUS_HELPERS_SOCKET_HEADER_
#define DCMTK_LOG4CPLUS_HELPERS_SOCKET_HEADER_

#include "dcmtk/oflog/config.h"
#include "dcmtk/oflog/tstring.h"
#include "dcmtk/oflog/helpers/sockbuff.h"
#if defined(_WIN32)
#include <winsock.h>
#endif

namespace dcmtk {
namespace log4cplus {
    namespace helpers {

        enum SocketState { ok,
                           not_opened,
                           bad_address,
                           connection_failed,
                           broken_pipe,
                           invalid_access_mode,
                           message_truncated
                         };

#if !defined(_WIN32)
        typedef int SOCKET_TYPE;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#else
        typedef SOCKET SOCKET_TYPE;
#endif

        class DCMTK_LOG4CPLUS_EXPORT AbstractSocket {
        public:
          // ctor and dtor
            AbstractSocket();
            AbstractSocket(SOCKET_TYPE sock, SocketState state, int err);
            AbstractSocket(const AbstractSocket&);
            virtual ~AbstractSocket() = 0;

          // methods
            /// Close socket
            virtual void close();
            virtual bool isOpen() const;

            AbstractSocket& operator=(const AbstractSocket& rhs);

        protected:
          // Methods
            virtual void copy(const AbstractSocket& rhs);

          // Data
            SOCKET_TYPE sock;
            SocketState state;
            int err;
        };



        /**
         * This class implements client sockets (also called just "sockets").
         * A socket is an endpoint for communication between two machines.
         */
        class DCMTK_LOG4CPLUS_EXPORT Socket : public AbstractSocket {
        public:
          // ctor and dtor
            Socket();
            Socket(SOCKET_TYPE sock, SocketState state, int err);
            Socket(const tstring& address, int port);
            virtual ~Socket();

          // methods
            virtual bool read(SocketBuffer& buffer);
            virtual bool write(const SocketBuffer& buffer);
        };



        /**
         * This class implements server sockets. A server socket waits for
         * requests to come in over the network. It performs some operation
         * based on that request, and then possibly returns a result to the
         * requester.
         */
        class DCMTK_LOG4CPLUS_EXPORT ServerSocket : public AbstractSocket {
        public:
          // ctor and dtor
            ServerSocket(int port);
            virtual ~ServerSocket();

            Socket accept();
        };


        DCMTK_LOG4CPLUS_EXPORT SOCKET_TYPE openSocket(unsigned short port, SocketState& state);
        DCMTK_LOG4CPLUS_EXPORT SOCKET_TYPE connectSocket(const tstring& hostn,
                                                   unsigned short port, SocketState& state);
        DCMTK_LOG4CPLUS_EXPORT SOCKET_TYPE acceptSocket(SOCKET_TYPE sock, SocketState& state);
        DCMTK_LOG4CPLUS_EXPORT int closeSocket(SOCKET_TYPE sock);

        DCMTK_LOG4CPLUS_EXPORT long read(SOCKET_TYPE sock, SocketBuffer& buffer);
        DCMTK_LOG4CPLUS_EXPORT long write(SOCKET_TYPE sock, const SocketBuffer& buffer);

        DCMTK_LOG4CPLUS_EXPORT tstring getHostname (bool fqdn);

    } // end namespace helpers
} // end namespace log4cplus
} // end namespace dcmtk

#endif // DCMTK_LOG4CPLUS_HELPERS_SOCKET_HEADER_
