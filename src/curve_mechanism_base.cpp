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


#include "precompiled.hpp"
#include "curve_mechanism_base.hpp"
#include "msg.hpp"
#include "wire.hpp"
#include "session_base.hpp"

#ifdef ZMQ_HAVE_CURVE
#ifdef ZMQ_USE_LIBSODIUM
#define COMPILER_ASSERT(X) (void) sizeof (char[(X) ? 1 : -1])

static int zmq_crypto_secretbox_detached (unsigned char *c,
                                          unsigned char *mac,
                                          const unsigned char *m,
                                          unsigned long long mlen,
                                          const unsigned char *n,
                                          const unsigned char *k)
{
    crypto_onetimeauth_poly1305_state state;
    unsigned char block0[64U];
    unsigned char subkey[crypto_stream_salsa20_KEYBYTES];
    unsigned long long i;

    crypto_core_hsalsa20 (subkey, n, k, NULL);

    if ((c > m && static_cast<unsigned long long> (c - m) < mlen)
        || (m > c
            && static_cast<unsigned long long> (m - c)
                 < mlen)) { /* LCOV_EXCL_LINE */
        memmove (c, m, mlen);
        m = c;
    }
    memset (block0, 0U, crypto_secretbox_ZEROBYTES);
    COMPILER_ASSERT (64U >= crypto_secretbox_ZEROBYTES);
    unsigned long long mlen0 = mlen;
    if (mlen0 > 64U - crypto_secretbox_ZEROBYTES) {
        mlen0 = 64U - crypto_secretbox_ZEROBYTES;
    }
    for (i = 0U; i < mlen0; i++) {
        block0[i + crypto_secretbox_ZEROBYTES] = m[i];
    }
    crypto_stream_salsa20_xor (
      block0, block0, mlen0 + crypto_secretbox_ZEROBYTES, n + 16, subkey);
    COMPILER_ASSERT (crypto_secretbox_ZEROBYTES
                     >= crypto_onetimeauth_poly1305_KEYBYTES);
    crypto_onetimeauth_poly1305_init (&state, block0);

    for (i = 0U; i < mlen0; i++) {
        c[i] = block0[crypto_secretbox_ZEROBYTES + i];
    }
    sodium_memzero (block0, sizeof block0);
    if (mlen > mlen0) {
        crypto_stream_salsa20_xor_ic (c + mlen0, m + mlen0, mlen - mlen0,
                                      n + 16, 1U, subkey);
    }
    sodium_memzero (subkey, sizeof subkey);

    crypto_onetimeauth_poly1305_update (&state, c, mlen);
    crypto_onetimeauth_poly1305_final (&state, mac);
    sodium_memzero (&state, sizeof state);

    return 0;
}

static int zmq_crypto_secretbox_open_detached (unsigned char *m,
                                               const unsigned char *c,
                                               const unsigned char *mac,
                                               unsigned long long clen,
                                               const unsigned char *n,
                                               const unsigned char *k)
{
    unsigned char block0[64U];
    unsigned char subkey[crypto_stream_salsa20_KEYBYTES];
    unsigned long long i;

    crypto_core_hsalsa20 (subkey, n, k, NULL);
    crypto_stream_salsa20 (block0, crypto_stream_salsa20_KEYBYTES, n + 16,
                           subkey);
    if (crypto_onetimeauth_poly1305_verify (mac, c, clen, block0) != 0) {
        sodium_memzero (subkey, sizeof subkey);
        return -1;
    }
    if (m == NULL) {
        return 0;
    }
    if ((c >= m && static_cast<unsigned long long> (c - m) < clen)
        || (m >= c
            && static_cast<unsigned long long> (m - c)
                 < clen)) { /* LCOV_EXCL_LINE */
        memmove (m, c, clen);
        c = m;
    }
    unsigned long long mlen0 = clen;
    if (mlen0 > 64U - crypto_secretbox_ZEROBYTES) {
        mlen0 = 64U - crypto_secretbox_ZEROBYTES;
    }
    for (i = 0U; i < mlen0; i++) {
        block0[crypto_secretbox_ZEROBYTES + i] = c[i];
    }
    crypto_stream_salsa20_xor (
      block0, block0, crypto_secretbox_ZEROBYTES + mlen0, n + 16, subkey);
    for (i = 0U; i < mlen0; i++) {
        m[i] = block0[i + crypto_secretbox_ZEROBYTES];
    }
    if (clen > mlen0) {
        crypto_stream_salsa20_xor_ic (m + mlen0, c + mlen0, clen - mlen0,
                                      n + 16, 1U, subkey);
    }
    sodium_memzero (subkey, sizeof subkey);

    return 0;
}


static int zmq_crypto_box_detached_afternm (unsigned char *c,
                                            unsigned char *mac,
                                            const unsigned char *m,
                                            unsigned long long mlen,
                                            const unsigned char *n,
                                            const unsigned char *k)
{
    return zmq_crypto_secretbox_detached (c, mac, m, mlen, n, k);
}

static int zmq_crypto_box_easy_afternm (unsigned char *c,
                                        const unsigned char *m,
                                        unsigned long long mlen,
                                        const unsigned char *n,
                                        const unsigned char *k)
{
    if (mlen > crypto_box_MESSAGEBYTES_MAX) {
        sodium_misuse ();
    }
    return zmq_crypto_box_detached_afternm (c + crypto_box_MACBYTES, c, m, mlen,
                                            n, k);
}

static int zmq_crypto_box_open_detached_afternm (unsigned char *m,
                                                 const unsigned char *c,
                                                 const unsigned char *mac,
                                                 unsigned long long clen,
                                                 const unsigned char *n,
                                                 const unsigned char *k)
{
    return zmq_crypto_secretbox_open_detached (m, c, mac, clen, n, k);
}

static int zmq_crypto_box_open_easy_afternm (unsigned char *m,
                                             const unsigned char *c,
                                             unsigned long long clen,
                                             const unsigned char *n,
                                             const unsigned char *k)
{
    if (clen < crypto_box_MACBYTES) {
        return -1;
    }
    return zmq_crypto_box_open_detached_afternm (
      m, c + crypto_box_MACBYTES, c, clen - crypto_box_MACBYTES, n, k);
}

#endif

zmq::curve_mechanism_base_t::curve_mechanism_base_t (
  session_base_t *session_,
  const options_t &options_,
  const char *encode_nonce_prefix_,
  const char *decode_nonce_prefix_) :
    mechanism_base_t (session_, options_),
    curve_encoding_t (encode_nonce_prefix_, decode_nonce_prefix_)
{
}

int zmq::curve_mechanism_base_t::encode (msg_t *msg_)
{
    return curve_encoding_t::encode (msg_);
}

int zmq::curve_mechanism_base_t::decode (msg_t *msg_)
{
    int rc = check_basic_command_structure (msg_);
    if (rc == -1)
        return -1;

    int error_event_code;
    rc = curve_encoding_t::decode (msg_, &error_event_code);
    if (-1 == rc) {
        session->get_socket ()->event_handshake_failed_protocol (
          session->get_endpoint (), error_event_code);
    }

    return rc;
}

zmq::curve_encoding_t::curve_encoding_t (const char *encode_nonce_prefix_,
                                         const char *decode_nonce_prefix_) :
    _encode_nonce_prefix (encode_nonce_prefix_),
    _decode_nonce_prefix (decode_nonce_prefix_),
    _cn_nonce (1),
    _cn_peer_nonce (1)
{
}

//  Right now, we only transport the lower two bit flags of zmq::msg_t, so they
//  are binary identical, and we can just use a bitmask to select them. If we
//  happened to add more flags, this might change.
static const uint8_t flag_mask = zmq::msg_t::more | zmq::msg_t::command;
static const size_t flags_len = 1;
static const size_t nonce_prefix_len = 16;
static const char message_command[] = "\x07MESSAGE";
static const size_t message_command_len = sizeof (message_command) - 1;
static const size_t message_header_len =
  message_command_len + sizeof (zmq::curve_encoding_t::nonce_t);

#ifndef ZMQ_USE_LIBSODIUM
static const size_t crypto_box_MACBYTES = 16;
#endif

int zmq::curve_encoding_t::check_validity (msg_t *msg_, int *error_event_code_)
{
    const size_t size = msg_->size ();
    const uint8_t *const message = static_cast<uint8_t *> (msg_->data ());

    if (size < message_command_len
        || 0 != memcmp (message, message_command, message_command_len)) {
        *error_event_code_ = ZMQ_PROTOCOL_ERROR_ZMTP_UNEXPECTED_COMMAND;
        errno = EPROTO;
        return -1;
    }

    if (size < message_header_len + crypto_box_MACBYTES + flags_len) {
        *error_event_code_ = ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_MESSAGE;
        errno = EPROTO;
        return -1;
    }

    {
        const uint64_t nonce = get_uint64 (message + message_command_len);
        if (nonce <= _cn_peer_nonce) {
            *error_event_code_ = ZMQ_PROTOCOL_ERROR_ZMTP_INVALID_SEQUENCE;
            errno = EPROTO;
            return -1;
        }
        set_peer_nonce (nonce);
    }

    return 0;
}

int zmq::curve_encoding_t::encode (msg_t *msg_)
{
    uint8_t message_nonce[crypto_box_NONCEBYTES];
    memcpy (message_nonce, _encode_nonce_prefix, nonce_prefix_len);
    put_uint64 (message_nonce + nonce_prefix_len, get_and_inc_nonce ());

#ifdef ZMQ_USE_LIBSODIUM
    const size_t mlen = flags_len + msg_->size ();
    std::vector<uint8_t> message_plaintext (mlen);
#else
    const size_t mlen = crypto_box_ZEROBYTES + flags_len + msg_->size ();
    std::vector<uint8_t> message_plaintext_with_zerobytes (mlen);
    uint8_t *const message_plaintext =
      &message_plaintext_with_zerobytes[crypto_box_ZEROBYTES];

    std::fill (message_plaintext_with_zerobytes.begin (),
               message_plaintext_with_zerobytes.begin () + crypto_box_ZEROBYTES,
               0);
#endif

    const uint8_t flags = msg_->flags () & flag_mask;
    message_plaintext[0] = flags;
    // this is copying the data from insecure memory, so there is no point in
    // using secure_allocator_t for message_plaintext
    if (msg_->size () > 0)
        memcpy (&message_plaintext[flags_len], msg_->data (), msg_->size ());

#ifdef ZMQ_USE_LIBSODIUM
    msg_t msg_box;
    int rc =
      msg_box.init_size (message_header_len + mlen + crypto_box_MACBYTES);
    zmq_assert (rc == 0);

    rc = zmq_crypto_box_easy_afternm (
      static_cast<uint8_t *> (msg_box.data ()) + message_header_len,
      &message_plaintext[0], mlen, message_nonce, _cn_precom);
    zmq_assert (rc == 0);

    msg_->move (msg_box);

    uint8_t *const message = static_cast<uint8_t *> (msg_->data ());
#else
    std::vector<uint8_t> message_box (mlen);

    int rc =
      crypto_box_afternm (&message_box[0], &message_plaintext_with_zerobytes[0],
                          mlen, message_nonce, _cn_precom);
    zmq_assert (rc == 0);

    rc = msg_->close ();
    zmq_assert (rc == 0);

    rc = msg_->init_size (16 + mlen - crypto_box_BOXZEROBYTES);
    zmq_assert (rc == 0);

    uint8_t *const message = static_cast<uint8_t *> (msg_->data ());

    memcpy (message + message_header_len, &message_box[crypto_box_BOXZEROBYTES],
            mlen - crypto_box_BOXZEROBYTES);
#endif

    memcpy (message, message_command, message_command_len);
    memcpy (message + message_command_len, message_nonce + nonce_prefix_len,
            sizeof (nonce_t));

    return 0;
}

int zmq::curve_encoding_t::decode (msg_t *msg_, int *error_event_code_)
{
    int rc = check_validity (msg_, error_event_code_);
    if (0 != rc) {
        return rc;
    }

    uint8_t *const message = static_cast<uint8_t *> (msg_->data ());

    uint8_t message_nonce[crypto_box_NONCEBYTES];
    memcpy (message_nonce, _decode_nonce_prefix, nonce_prefix_len);
    memcpy (message_nonce + nonce_prefix_len, message + message_command_len,
            sizeof (nonce_t));

#ifdef ZMQ_USE_LIBSODIUM
    const size_t clen = msg_->size () - message_header_len;

    uint8_t *const message_plaintext = message + message_header_len;

    rc = zmq_crypto_box_open_easy_afternm (message_plaintext,
                                           message + message_header_len, clen,
                                           message_nonce, _cn_precom);
#else
    const size_t clen =
      crypto_box_BOXZEROBYTES + msg_->size () - message_header_len;

    std::vector<uint8_t> message_plaintext_with_zerobytes (clen);
    std::vector<uint8_t> message_box (clen);

    std::fill (message_box.begin (),
               message_box.begin () + crypto_box_BOXZEROBYTES, 0);
    memcpy (&message_box[crypto_box_BOXZEROBYTES], message + message_header_len,
            msg_->size () - message_header_len);

    rc = crypto_box_open_afternm (&message_plaintext_with_zerobytes[0],
                                  &message_box[0], clen, message_nonce,
                                  _cn_precom);

    const uint8_t *const message_plaintext =
      &message_plaintext_with_zerobytes[crypto_box_ZEROBYTES];
#endif

    if (rc == 0) {
        const uint8_t flags = message_plaintext[0];

#ifdef ZMQ_USE_LIBSODIUM
        const size_t plaintext_size = clen - flags_len - crypto_box_MACBYTES;

        if (plaintext_size > 0) {
            memmove (msg_->data (), &message_plaintext[flags_len],
                     plaintext_size);
        }

        msg_->shrink (plaintext_size);
#else
        rc = msg_->close ();
        zmq_assert (rc == 0);

        rc = msg_->init_size (clen - flags_len - crypto_box_ZEROBYTES);
        zmq_assert (rc == 0);

        // this is copying the data to insecure memory, so there is no point in
        // using secure_allocator_t for message_plaintext
        if (msg_->size () > 0) {
            memcpy (msg_->data (), &message_plaintext[flags_len],
                    msg_->size ());
        }
#endif

        msg_->set_flags (flags & flag_mask);
    } else {
        // CURVE I : connection key used for MESSAGE is wrong
        *error_event_code_ = ZMQ_PROTOCOL_ERROR_ZMTP_CRYPTOGRAPHIC;
        errno = EPROTO;
    }

    return rc;
}

#endif
