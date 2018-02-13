/*
    Copyright (c) 2007-2017 Contributors as noted in the AUTHORS file

    This file is part of libzmq, the ZeroMQ core engine in C++.

    libzmq is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, the Contributors give you permission to link
    this library with independent modules to produce an executable,
    regardless of the license terms of these independent modules, and to
    copy and distribute the resulting executable under terms of your choice,
    provided that you also meet, for each linked independent module, the
    terms and conditions of the license of that module. An independent
    module is a module which is not derived from or based on this library.
    If you modify this library, you must extend this exception to your
    version of the library.

    libzmq is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "testutil.hpp"

#include <unity.h>

void setUp ()
{
}
void tearDown ()
{
}

// const int MAX_SENDS = 10000;

int test_defaults (int send_hwm, int msgCnt)
{
    void *ctx = zmq_ctx_new ();
    assert (ctx);
    int rc;

    // Set up bind socket
    void *pub_socket = zmq_socket (ctx, ZMQ_PUB);
    assert (pub_socket);
    rc = zmq_bind (pub_socket, "inproc://a");
    assert (rc == 0);

    // Set up connect socket
    void *sub_socket = zmq_socket (ctx, ZMQ_SUB);
    assert (sub_socket);
    rc = zmq_connect (sub_socket, "inproc://a");
    assert (rc == 0);

    //set a hwm on publisher
    rc = zmq_setsockopt (pub_socket, ZMQ_SNDHWM, &send_hwm, sizeof (send_hwm));
    rc = zmq_setsockopt (sub_socket, ZMQ_SUBSCRIBE, 0, 0);

    // Send until we block
    int send_count = 0;
    while (send_count < msgCnt
           && zmq_send (pub_socket, NULL, 0, ZMQ_DONTWAIT) == 0)
        ++send_count;

    msleep (SETTLE_TIME);

    // Now receive all sent messages
    int recv_count = 0;
    while (0 == zmq_recv (sub_socket, NULL, 0, ZMQ_DONTWAIT)) {
        ++recv_count;
    }

    assert (send_hwm == recv_count);

    // Clean up
    rc = zmq_close (sub_socket);
    assert (rc == 0);

    rc = zmq_close (pub_socket);
    assert (rc == 0);

    rc = zmq_ctx_term (ctx);
    assert (rc == 0);

    return recv_count;
}

int receive (void *socket)
{
    int recv_count = 0;
    // Now receive all sent messages
    while (0 == zmq_recv (socket, NULL, 0, ZMQ_DONTWAIT)) {
        ++recv_count;
    }

    return recv_count;
}


int test_blocking (int send_hwm, int msgCnt)
{
    void *ctx = zmq_ctx_new ();
    assert (ctx);
    int rc;

    // Set up bind socket
    void *pub_socket = zmq_socket (ctx, ZMQ_PUB);
    assert (pub_socket);
    rc = zmq_bind (pub_socket, "inproc://a");
    assert (rc == 0);

    // Set up connect socket
    void *sub_socket = zmq_socket (ctx, ZMQ_SUB);
    assert (sub_socket);
    rc = zmq_connect (sub_socket, "inproc://a");
    assert (rc == 0);

    //set a hwm on publisher
    rc = zmq_setsockopt (pub_socket, ZMQ_SNDHWM, &send_hwm, sizeof (send_hwm));
    int wait = 1;
    rc = zmq_setsockopt (pub_socket, ZMQ_XPUB_NODROP, &wait, sizeof (wait));
    rc = zmq_setsockopt (sub_socket, ZMQ_SUBSCRIBE, 0, 0);

    // Send until we block
    int send_count = 0;
    int recv_count = 0;
    while (send_count < msgCnt) {
        rc = zmq_send (pub_socket, NULL, 0, ZMQ_DONTWAIT);
        if (rc == 0) {
            ++send_count;
        } else if (-1 == rc) {
            assert (EAGAIN == errno);
            recv_count += receive (sub_socket);
            assert (recv_count == send_count);
        }
    }

    recv_count += receive (sub_socket);

    // Clean up
    rc = zmq_close (sub_socket);
    assert (rc == 0);

    rc = zmq_close (pub_socket);
    assert (rc == 0);

    rc = zmq_ctx_term (ctx);
    assert (rc == 0);

    return recv_count;
}

// with hwm 11024: send 9999 msg, receive 9999, send 1100, receive 1100
void test_reset_hwm ()
{
    int first_count = 9999;
    int second_count = 1100;
    int hwm = 11024;
    size_t len = MAX_SOCKET_STRING;
    char my_endpoint[MAX_SOCKET_STRING];

    void *ctx = zmq_ctx_new ();
    assert (ctx);
    int rc;

    // Set up bind socket
    void *pub_socket = zmq_socket (ctx, ZMQ_PUB);
    assert (pub_socket);
    rc = zmq_setsockopt (pub_socket, ZMQ_SNDHWM, &hwm, sizeof (hwm));
    assert (rc == 0);
    rc = zmq_bind (pub_socket, "tcp://127.0.0.1:*");
    assert (rc == 0);
    rc = zmq_getsockopt (pub_socket, ZMQ_LAST_ENDPOINT, my_endpoint, &len);
    assert (rc == 0);

    // Set up connect socket
    void *sub_socket = zmq_socket (ctx, ZMQ_SUB);
    assert (sub_socket);
    rc = zmq_setsockopt (sub_socket, ZMQ_RCVHWM, &hwm, sizeof (hwm));
    assert (rc == 0);
    rc = zmq_connect (sub_socket, my_endpoint);
    assert (rc == 0);
    rc = zmq_setsockopt (sub_socket, ZMQ_SUBSCRIBE, 0, 0);
    assert (rc == 0);

    msleep (SETTLE_TIME);

    // Send messages
    int send_count = 0;
    while (send_count < first_count
           && zmq_send (pub_socket, NULL, 0, ZMQ_DONTWAIT) == 0)
        ++send_count;
    assert (first_count == send_count);

    msleep (SETTLE_TIME);

    // Now receive all sent messages
    int recv_count = 0;
    while (0 == zmq_recv (sub_socket, NULL, 0, ZMQ_DONTWAIT)) {
        ++recv_count;
    }
    assert (first_count == recv_count);

    msleep (SETTLE_TIME);

    // Send messages
    send_count = 0;
    while (send_count < second_count
           && zmq_send (pub_socket, NULL, 0, ZMQ_DONTWAIT) == 0)
        ++send_count;
    assert (second_count == send_count);

    msleep (SETTLE_TIME);

    // Now receive all sent messages
    recv_count = 0;
    while (0 == zmq_recv (sub_socket, NULL, 0, ZMQ_DONTWAIT)) {
        ++recv_count;
    }
    assert (second_count == recv_count);

    // Clean up
    rc = zmq_close (sub_socket);
    assert (rc == 0);

    rc = zmq_close (pub_socket);
    assert (rc == 0);

    rc = zmq_ctx_term (ctx);
    assert (rc == 0);
}

void test_defaults_1000_1000 ()
{
    int count;

    // send 1000 msg on hwm 1000, receive 1000
    count = test_defaults (1000, 1000);
    assert (count == 1000);
}

void test_blocking_2000_6000 ()
{
    int count;
    // send 6000 msg on hwm 2000, drops above hwm, only receive hwm
    count = test_blocking (2000, 6000);
    assert (count == 6000);
}

void gen_topic (int n, char *topic)
{
    // Simple hash function to generate a subscription prefix from a number.
    n = (n * 2654435761);
    sprintf (topic, "%08x", n);
}

void getsockopt_events_within_many_subscriptions (void *sub, int i)
{
    char topic[8];
    char opt[256];
    size_t opt_len = 256;

    int n = 10000;

    for (int j = 0; j < n; ++j) {
        gen_topic (j, topic);
        zmq_setsockopt (sub, ZMQ_SUBSCRIBE, &topic, 8);
        // CRASH: Get ZMQ_EVENTS from a SUB socket.
        zmq_getsockopt (sub, ZMQ_EVENTS, opt, &opt_len);
    }
    for (int j = 0; j < n; ++j) {
        gen_topic (j, topic);
        zmq_setsockopt (sub, ZMQ_UNSUBSCRIBE, &topic, 8);
        // CRASH: Get ZMQ_EVENTS from a SUB socket.
        zmq_getsockopt (sub, ZMQ_EVENTS, opt, &opt_len);
    }
}

void test_issue_2943 ()
{
    void *context = zmq_ctx_new ();
    void *pub = zmq_socket (context, ZMQ_PUB);
    void *sub;

    char addr[256];
    size_t addr_len = 256;
    char opt[256];
    size_t opt_len = 256;
    int sub_hwm = 1;

    zmq_bind (pub, "tcp://127.0.0.1:*");
    zmq_getsockopt (pub, ZMQ_LAST_ENDPOINT, addr, &addr_len);

    for (int i = 0; i < 100; ++i) {
        sub = zmq_socket (context, ZMQ_SUB);
        zmq_setsockopt (sub, ZMQ_SNDHWM, &sub_hwm, sizeof (sub_hwm));

        zmq_connect (sub, addr);
        zmq_getsockopt (pub, ZMQ_EVENTS, opt, &opt_len);

        getsockopt_events_within_many_subscriptions (sub, i);
    }
}

int main (void)
{
    setup_test_environment ();

    UNITY_BEGIN ();
    RUN_TEST (test_defaults_1000_1000);
    RUN_TEST (test_blocking_2000_6000);

    // hwm should apply to the messages that have already been received
    RUN_TEST (test_reset_hwm);

    RUN_TEST (test_issue_2943);
    return UNITY_END ();
}
