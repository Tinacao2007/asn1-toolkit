/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/common/span.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Span<T> non-owning buffer view (C++17 replacement for std::span).
**
** Specification: No external protocol; C++ infrastructure shared by
**                 compiler and runtime.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace asn1 {

/// C++17 stand-in for std::span. Non-owning view over a contiguous range.
template <typename T>
class Span {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using iterator = T*;
  using const_iterator = const T*;

  constexpr Span() noexcept : data_(nullptr), size_(0) {}
  constexpr Span(T* data, size_type size) noexcept : data_(data), size_(size) {}

  template <typename Container,
            typename = std::void_t<decltype(std::declval<Container&>().data()),
                                   decltype(std::declval<Container&>().size())>>
  constexpr Span(Container& c) noexcept : data_(c.data()), size_(c.size()) {}

  template <typename Container,
            typename = std::void_t<decltype(std::declval<const Container&>().data()),
                                   decltype(std::declval<const Container&>().size())>,
            typename = std::enable_if_t<std::is_const_v<T>>>
  constexpr Span(const Container& c) noexcept : data_(c.data()), size_(c.size()) {}

  template <std::size_t N>
  constexpr Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

  constexpr pointer data() const noexcept { return data_; }
  constexpr size_type size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr reference operator[](size_type i) const noexcept { return data_[i]; }

  constexpr iterator begin() const noexcept { return data_; }
  constexpr iterator end() const noexcept { return data_ + size_; }
  constexpr const_iterator cbegin() const noexcept { return data_; }
  constexpr const_iterator cend() const noexcept { return data_ + size_; }

  constexpr Span subspan(size_type offset,
                         size_type count = static_cast<size_type>(-1)) const noexcept {
    const size_type remaining = size_ > offset ? size_ - offset : 0;
    const size_type n =
        (count == static_cast<size_type>(-1) || count > remaining) ? remaining : count;
    /**
     *  Function    : +
     *  Description : Computes + from (size_), n).
     *  Parameters  : size_) — data_ + (offset < size_ ? offset : size_); n — n
     *  Returns     : return Span(data_
     */
    return Span(data_ + (offset < size_ ? offset : size_), n);
  }

 private:
  pointer data_;
  size_type size_;
};

}  // namespace asn1
