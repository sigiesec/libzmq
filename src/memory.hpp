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

#ifndef __ZMQ_MEMORY_INCLUDED__
#define __ZMQ_MEMORY_INCLUDED__

#include "err.hpp"
#include "macros.hpp"

namespace zmq
{
enum ScopedPtrPolicy
{
    ScopedPtrPolicy_Unmodifiable,
    ScopedPtrPolicy_Modifiable,
    ScopedPtrPolicy_Releasable
};

template <typename T> class unmodifiable_scoped_ptr_t
{
  public:
    T *operator-> () const { return _ptr; }
    T *get () const { return _ptr; }

    explicit operator bool () const { return _ptr != ZMQ_NULL; }

    ~unmodifiable_scoped_ptr_t () { reset_internal (); }

  protected:
    unmodifiable_scoped_ptr_t () : _ptr (ZMQ_NULL) {}

    unmodifiable_scoped_ptr_t (T *const ptr_) : _ptr (ptr_)
    {
        alloc_assert (_ptr);
    }


    void reset_internal ()
    {
        delete _ptr;
        _ptr = ZMQ_NULL;
    }

    T *_ptr;

  private:
    ZMQ_NON_COPYABLE_NOR_MOVABLE (unmodifiable_scoped_ptr_t)
};

template <typename T, ScopedPtrPolicy Policy = ScopedPtrPolicy_Unmodifiable>
class scoped_ptr_t;

template <typename T>
class scoped_ptr_t<T, ScopedPtrPolicy_Unmodifiable> ZMQ_FINAL
    : public unmodifiable_scoped_ptr_t<T>
{
  public:
    scoped_ptr_t (T *const ptr_) : unmodifiable_scoped_ptr_t (ptr_) {}
};

template <typename T>
class scoped_ptr_t<T, ScopedPtrPolicy_Modifiable>
    : public unmodifiable_scoped_ptr_t<T>
{
  public:
    scoped_ptr_t () ZMQ_DEFAULT;

    scoped_ptr_t (T *const ptr_) : unmodifiable_scoped_ptr_t (ptr_) {}

    void init (T *const ptr_)
    {
        zmq_assert (!_ptr);
        _ptr = ptr_;
        alloc_assert (_ptr);
    }

    void reset () { reset_internal (); }
};

template <typename T>
class scoped_ptr_t<T, ScopedPtrPolicy_Releasable> ZMQ_FINAL
    : public scoped_ptr_t<T, ScopedPtrPolicy_Modifiable>
{
  public:
    scoped_ptr_t () ZMQ_DEFAULT;

    scoped_ptr_t (T *const ptr_) :
        scoped_ptr_t<T, ScopedPtrPolicy_Modifiable> (ptr_)
    {
    }

    T *release ()
    {
        T *const res = get ();
        _ptr = ZMQ_NULL;
        return res;
    }
};


}

#endif
