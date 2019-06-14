// Filename: FixedSizedStream-inl.h
// Created on 14 Jun 2004 by Boyce Griffith
//
// Copyright (c) 2002-2017, Boyce Griffith
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of The University of North Carolina nor the names of
//      its contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef included_IBTK_FixedSizedStream_inl_h
#define included_IBTK_FixedSizedStream_inl_h

/////////////////////////////// INCLUDES /////////////////////////////////////

#include "ibtk/FixedSizedStream.h"

#include "tbox/Utilities.h"

#include <cstring>

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// PUBLIC ///////////////////////////////////////

inline void*
FixedSizedStream::getBufferStart()
{
    return static_cast<void*>(&d_buffer[0]);
} // getBufferStart

inline const void*
FixedSizedStream::getBufferStart() const
{
    return static_cast<const void*>(&d_buffer[0]);
} // getBufferStart

inline int
FixedSizedStream::getCurrentSize() const
{
    return d_current_size;
} // getCurrentSize

inline int
FixedSizedStream::getCurrentIndex() const
{
    return d_buffer_index;
} // getCurrentIndex

inline void
FixedSizedStream::setCurrentIndex(const int index)
{
    d_buffer_index = index;
    return;
} // setCurrentIndex

inline void
FixedSizedStream::resetIndex()
{
    setCurrentIndex(0);
    return;
} // resetIndex

/*
*************************************************************************
*                                                                       *
* Packing and unpacking member functions for booleans.  Note that since *
* the boolean representation is non-standard, boolean arrays are copied *
* by converting into character arrays.                                  *
*                                                                       *
*************************************************************************
*/

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator<<(const bool& data)
{
    pack(&data, 1);
    return *this;
} // operator<<

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator>>(bool& data)
{
    unpack(&data, 1);
    return *this;
} // operator>>

inline void
FixedSizedStream::pack(const bool* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofBool(n);
    void* ptr = getPointerAndAdvanceCursor(bytes);
    char* c_ptr = static_cast<char*>(ptr);
    for (int i = 0; i < n; i++)
    {
        c_ptr[i] = (data[i] ? 1 : 0);
    }
    return;
} // pack

inline void
FixedSizedStream::unpack(bool* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofBool(n);
    void* ptr = getPointerAndAdvanceCursor(bytes);
    const char* c_ptr = static_cast<const char*>(ptr);
    for (int i = 0; i < n; i++)
    {
        data[i] = (c_ptr[i] == 1);
    }
    return;
} // unpack

/*
*************************************************************************
*                                                                       *
* Packing and unpacking member functions for characters.                *
*                                                                       *
*************************************************************************
*/

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator<<(const char& data)
{
    pack(&data, 1);
    return *this;
} // operator<<

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator>>(char& data)
{
    unpack(&data, 1);
    return *this;
} // operator>>

inline void
FixedSizedStream::pack(const char* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofChar(n);
    __pack(data, bytes);
    return;
} // pack

inline void
FixedSizedStream::unpack(char* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofChar(n);
    __unpack(data, bytes);
    return;
} // unpack

/*
*************************************************************************
*                                                                       *
* Packing and unpacking member functions for double complex.            *
*                                                                       *
*************************************************************************
*/

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator<<(const dcomplex& data)
{
    pack(&data, 1);
    return *this;
} // operator<<

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator>>(dcomplex& data)
{
    unpack(&data, 1);
    return *this;
} // operator>>

inline void
FixedSizedStream::pack(const dcomplex* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofDoubleComplex(n);
    __pack(data, bytes);
    return;
} // pack

inline void
FixedSizedStream::unpack(dcomplex* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofDoubleComplex(n);
    __unpack(data, bytes);
    return;
} // unpack

/*
*************************************************************************
*                                                                       *
* Packing and unpacking member functions for doubles.                   *
*                                                                       *
*************************************************************************
*/

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator<<(const double& data)
{
    pack(&data, 1);
    return *this;
} // operator<<

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator>>(double& data)
{
    unpack(&data, 1);
    return *this;
} // operator>>

inline void
FixedSizedStream::pack(const double* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofDouble(n);
    __pack(data, bytes);
    return;
} // pack

inline void
FixedSizedStream::unpack(double* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofDouble(n);
    __unpack(data, bytes);
    return;
} // unpack

/*
*************************************************************************
*                                                                       *
* Packing and unpacking member functions for floats.                    *
*                                                                       *
*************************************************************************
*/

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator<<(const float& data)
{
    pack(&data, 1);
    return *this;
} // operator<<

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator>>(float& data)
{
    unpack(&data, 1);
    return *this;
} // operator>>

inline void
FixedSizedStream::pack(const float* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofFloat(n);
    __pack(data, bytes);
    return;
} // pack

inline void
FixedSizedStream::unpack(float* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofFloat(n);
    __unpack(data, bytes);
    return;
} // unpack

/*
*************************************************************************
*                                                                       *
* Packing and unpacking member functions for integers.                  *
*                                                                       *
*************************************************************************
*/

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator<<(const int& data)
{
    pack(&data, 1);
    return *this;
} // operator<<

inline SAMRAI::tbox::AbstractStream&
FixedSizedStream::operator>>(int& data)
{
    unpack(&data, 1);
    return *this;
} // operator>>

inline void
FixedSizedStream::pack(const int* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofInt(n);
    __pack(data, bytes);
    return;
} // pack

inline void
FixedSizedStream::unpack(int* data, const int n)
{
    const int bytes = SAMRAI::tbox::AbstractStream::sizeofInt(n);
    __unpack(data, bytes);
    return;
} // unpack

/////////////////////////////// PRIVATE //////////////////////////////////////

/*
*************************************************************************
*                                                                       *
* Packing/unpacking helper functions.  The member function              *
* getPointerAndAdvanceCursor() returns a pointer to buffer space and    *
* advances internal pointers to reflect the allocated buffers space.    *
* The two functions given below simplify packing and unpacking for      *
* the * numerous member functions below.                                *
*                                                                       *
*************************************************************************
*/

inline void*
FixedSizedStream::getPointerAndAdvanceCursor(const int bytes)
{
    void* ptr = &d_buffer[d_buffer_index];
    d_buffer_index += bytes;
    if (d_buffer_index > d_current_size)
    {
        d_current_size = d_buffer_index;
        if (d_buffer_index > d_buffer_size)
        {
            TBOX_ERROR("FixedSizedStream::getPointerAndAdvanceCursor():\n"
                       << "  buffer overrun." << std::endl);
        }
    }
    return ptr;
} // getPointerAndAdvanceCursor

template <typename T>
inline void
FixedSizedStream::__pack(const T* const m_data, unsigned int m_bytes)
{
    void* const ptr = getPointerAndAdvanceCursor(m_bytes);
    std::memcpy(ptr, static_cast<const void*>(m_data), m_bytes);
    return;
} // _pack

template <typename T>
inline void
FixedSizedStream::__unpack(T* const m_data, unsigned int m_bytes)
{
    const void* const ptr = getPointerAndAdvanceCursor(m_bytes);
    std::memcpy(static_cast<void*>(m_data), ptr, m_bytes);
    return;
} // _unpack

//////////////////////////////////////////////////////////////////////////////

} // namespace IBTK

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_IBTK_FixedSizedStream_inl_h
