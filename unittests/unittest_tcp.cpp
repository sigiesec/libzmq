/*
Copyright (c) 2018 Contributors as noted in the AUTHORS file

This file is part of 0MQ.

0MQ is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

0MQ is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../tests/testutil_unity.hpp"

#include <address.hpp>
#include <ip.hpp>
#include <options.hpp>
#include <tcp.hpp>
#include <tcp_address.hpp>

#include <unity.h>

#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#define closesocket close
#endif

void setUp ()
{
}

void tearDown ()
{
}

typedef void (*socket_pair_op_t) (fd_t, fd_t);

static void socket_pair_no_op (fd_t connect_fd_, fd_t bind_fd_)
{
}

static void socket_pair_no_accept_send (fd_t connect_fd_, fd_t bind_fd_)
{
    char buf[4096];
    memset (buf, 0, sizeof buf);
    TEST_ASSERT_SUCCESS_RAW_ERRNO (
      zmq::tcp_write (connect_fd_, buf, sizeof buf));
}

static void test_tcp_open_socket_pair (const zmq::options_t &connect_options_,
                                       const zmq::options_t &bind_options_,
                                       socket_pair_op_t socket_pair_op_)
{
    zmq::tcp_address_t bind_tcp_address;
    zmq::fd_t bind_fd = zmq::tcp_open_socket ("127.0.0.1:*", bind_options_,
                                              true, true, &bind_tcp_address);
    TEST_ASSERT_NOT_EQUAL (zmq::retired_fd, bind_fd);

    zmq::unblock_socket (bind_fd);

#if defined ZMQ_HAVE_VXWORKS
    TEST_ASSERT_SUCCESS_RAW_ERRNO (bind (bind_fd,
                                         (sockaddr *) bind_tcp_address.addr (),
                                         bind_tcp_address.addrlen ()));
#else
    TEST_ASSERT_SUCCESS_RAW_ERRNO (
      bind (bind_fd, bind_tcp_address.addr (), bind_tcp_address.addrlen ()));
#endif

    sockaddr_storage bind_address;
    zmq::get_socket_address (bind_fd, zmq::socket_end_local, &bind_address);

    char connect_tcp_address_string[MAX_SOCKET_STRING];
    sprintf (connect_tcp_address_string, "127.0.0.1:%i",
             reinterpret_cast<const sockaddr_in *> (&bind_address)->sin_port);

    zmq::tcp_address_t connect_tcp_address;
    zmq::fd_t connect_fd =
      zmq::tcp_open_socket (connect_tcp_address_string, connect_options_, false,
                            true, &connect_tcp_address);
    TEST_ASSERT_NOT_EQUAL (zmq::retired_fd, connect_fd);


    int rc;
#if defined ZMQ_HAVE_VXWORKS
    rc = ::connect (connect_fd, (sockaddr *) connect_tcp_address.addr (),
                    connect_tcp_address.addrlen ());
#else
    rc = ::connect (connect_fd, connect_tcp_address.addr (),
                    connect_tcp_address.addrlen ());
#endif
#if 0
    if (rc != 0) {
#ifdef ZMQ_HAVE_WINDOWS
        const int last_error = WSAGetLastError ();
        TEST_ASSERT_TRUE (last_error == WSAEINPROGRESS
                          || last_error == WSAEWOULDBLOCK);
#else
        TEST_ASSERT_TRUE (errno == EINTR || errno == EINPROGRESS);
#endif
    }
#else
    TEST_ASSERT_SUCCESS_RAW_ERRNO (rc);
#endif

    zmq::unblock_socket (connect_fd);

    socket_pair_op_ (connect_fd, bind_fd);

    closesocket (connect_fd);
    closesocket (bind_fd);
}

void test_tcp_open_socket_pair_default_options_no_op ()
{
    test_tcp_open_socket_pair (zmq::options_t (), zmq::options_t (),
                               socket_pair_no_op);
}

void test_tcp_open_socket_pair_zero_sndbuf_options_no_accept_send ()
{
    zmq::options_t zero_sndbuf_options;
    zero_sndbuf_options.sndbuf = 0;
    test_tcp_open_socket_pair (zero_sndbuf_options, zero_sndbuf_options,
                               socket_pair_no_accept_send);
}

int main ()
{
    setup_test_environment ();

    UNITY_BEGIN ();
    RUN_TEST (test_tcp_open_socket_pair_default_options_no_op);
    RUN_TEST (test_tcp_open_socket_pair_zero_sndbuf_options_no_accept_send);
    return UNITY_END ();
}
