/*
    Copyright (c) 2007-2016 Contributors as noted in the AUTHORS file

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

#include <stdio.h>

void test_bind_invalid_address ()
{
    void *ctx = zmq_ctx_new ();
    assert (ctx);
    int rc;

    void *bind_socket = zmq_socket (ctx, ZMQ_PUB);
    assert (bind_socket);

    rc = zmq_bind (bind_socket, "epgm://239.192.1.1:5001");
	if (rc == -1) fprintf (stderr, "bind failed: %i\n", errno);
    assert (rc == -1 && errno == EINVAL);
    
	rc = zmq_ctx_destroy (&ctx);
    assert (rc == 0);
}

void test_connect_invalid_address ()
{
    void *ctx = zmq_ctx_new ();
    assert (ctx);
    int rc;

    void *connect_socket = zmq_socket (ctx, ZMQ_PUB);
    assert (connect_socket);

    rc = zmq_connect (connect_socket, "epgm://239.192.1.1:5001");
    if (rc == -1) fprintf (stderr, "connect failed: %i\n", errno);
    assert (rc == -1 && errno == EINVAL);
    
	rc = zmq_ctx_destroy (&ctx);
    assert (rc == 0);
}

void test_send_invalid_address ()
{
    void *ctx = zmq_ctx_new ();
    assert (ctx);
    int rc;

    void *connect_socket = zmq_socket (ctx, ZMQ_PUB);
    assert (connect_socket);

    rc = zmq_connect (connect_socket, "epgm://239.192.1.1:5001");
    if (rc == -1) fprintf (stderr, "zmq_connect failed: %i\n", errno);
    assert (rc == 0);	

    void *bind_socket = zmq_socket (ctx, ZMQ_SUB);
    assert (bind_socket);

    rc = zmq_bind (bind_socket, "epgm://239.192.1.1:5001");
	if (rc == -1) fprintf (stderr, "zmq_bind failed: %i\n", errno);
	assert (rc == 0);	

	rc = s_send (connect_socket, "FOO");
	assert (rc == 3);
	
	rc = zmq_close (bind_socket);
	assert (rc == 0);
	
	rc = zmq_close (connect_socket);
	assert (rc == 0);	
    
	rc = zmq_ctx_destroy (ctx);
	if (rc == -1) fprintf (stderr, "zmq_ctx_destroy failed: %i\n", errno);
    assert (rc == 0);
}


int main (void)
{
    if (!zmq_has ("pgm")) {
        printf ("PGM not installed, skipping test\n");
        return 77;
    }

    setup_test_environment ();
    
    test_send_invalid_address ();
    
    test_bind_invalid_address ();
    test_connect_invalid_address ();

    return 0;
}
