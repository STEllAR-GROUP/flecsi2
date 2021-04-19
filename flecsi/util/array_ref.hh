/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include "flecsi/util/target.hh"

namespace flecsi {
namespace util {

/// A workalike for std::span from C++20 (only dynamic-extent, without ranges
/// support).
template<class T>
struct span {
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;

  using iterator = pointer; // implementation-defined
  using reverse_iterator = std::reverse_iterator<iterator>;

  constexpr span() noexcept : span(nullptr, nullptr) {}
  constexpr span(pointer p, size_type sz) : span(p, p + sz) {}
  constexpr span(pointer p, pointer q) : p(p), q(q) {}
  template<std::size_t N>
  constexpr span(element_type (&a)[N]) : span(a, N) {}
  /// \warning Destroying \a C leaves this object dangling if it owns its
  ///   elements.  This implementation does not check for "borrowing".
  template<class C,
    class = std::enable_if_t<std::is_convertible_v<
      std::remove_pointer_t<decltype(void(std::size(std::declval<C &&>())),
        std::data(std::declval<C &&>()))> (*)[],
      T (*)[]>>>
  constexpr span(C && c) : span(std::data(c), std::size(c)) {}

  constexpr iterator begin() const noexcept {
    return p;
  }
  constexpr iterator end() const noexcept {
    return q;
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }
  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  constexpr reference front() const {
    return *begin();
  }
  constexpr reference back() const {
    return end()[-1];
  }

  constexpr reference operator[](size_type i) const {
    return begin()[i];
  }

  constexpr pointer data() const noexcept {
    return begin();
  }

  // FIXME: Spurious overflow for extremely large ranges
  constexpr size_type size() const noexcept {
    return end() - begin();
  }
  constexpr size_type size_bytes() const noexcept {
    return sizeof(element_type) * size();
  }

  constexpr bool empty() const noexcept {
    return begin() == end();
  }

  constexpr span first(size_type n) const {
    return {begin(), n};
  }
  constexpr span last(size_type n) const {
    return {end() - n, n};
  }
  constexpr span subspan(size_type i, size_type n = -1) const {
    return {begin() + i, n == size_type(-1) ? size() - i : n};
  }

private:
  pointer p, q;
};

template<class C>
span(C &)->span<typename C::value_type>;
template<class C>
span(const C &)->span<const typename C::value_type>;

/// Copy a span into a std::vector.
template<class T>
auto
to_vector(span<T> s) {
  // Work around GCC<8 having no deduction guide for vector:
  return std::vector<typename span<T>::value_type>(s.begin(), s.end());
}

/// A small, approximate subset of mdspan as proposed for C++23.
/// \tparam D dimension
template<class T, unsigned short D>
struct mdspan {
  static_assert(D > 0);
  using size_type = std::size_t;

  FLECSI_INLINE_TARGET
  constexpr mdspan(T * p, const size_type * sz) noexcept : p(p), strides() {
    for(int d = 0; d < D; d++)
      strides[d] = sz[d];

    for(int d = D - 1; d-- > 0;) // premultiply to convert to strides
      strides[d] *= strides[d + 1];
  }

  constexpr mdspan(T * p, const std::array<size_type, D> & sz) noexcept
    : mdspan(p, sz.data()) {}

  constexpr std::size_t extent(unsigned short i) const noexcept {
    return step(i) / step(i + 1);
  }

  template<class... I>
  constexpr decltype(auto) operator()(I... inds) const noexcept {
    static_assert(sizeof...(inds) == D);
    unsigned short d = D;
    size_type i = 0;
    ((i += size_type(inds) * step(d--), assert(size_type(inds) < extent(d))),
      ...); // guarantee evaluation order
    return p[i];
  }

  FLECSI_INLINE_TARGET
  constexpr decltype(auto) operator[](size_type i) const noexcept {
    assert(i < extent(0));
    const auto q = p + i * step(1);
    if constexpr(D > 1)
      return mdspan<T, D - 1>(q, &strides[1]);
    else
      return *q;
  }

private:
  constexpr size_type step(unsigned short i) const noexcept {
    assert(i <= D);
    return i == D ? 1 : strides[i];
  }

  T * p;
  size_type strides[D];
};

/// A very simple emulation of std::ranges::iota_view from C++20.
template<class I>
struct iota_view {
  struct iterator {
    using value_type = I;
    using reference = I;
    using pointer = void;
    using difference_type = I;
    using iterator_category = std::input_iterator_tag;

    constexpr iterator(I i = I()) : i(i) {}

    constexpr I operator*() const {
      return i;
    }
    constexpr I operator[](difference_type n) {
      return i + n;
    }

    constexpr iterator & operator++() {
      ++i;
      return *this;
    }
    constexpr iterator operator++(int) {
      const iterator ret = *this;
      ++*this;
      return ret;
    }
    constexpr iterator & operator--() {
      --i;
      return *this;
    }
    constexpr iterator operator--(int) {
      const iterator ret = *this;
      --*this;
      return ret;
    }
    constexpr iterator & operator+=(difference_type n) {
      i += n;
      return *this;
    }
    friend constexpr iterator operator+(difference_type n, iterator i) {
      i += n;
      return i;
    }
    constexpr iterator operator+(difference_type n) const {
      return n + *this;
    }
    constexpr iterator & operator-=(difference_type n) {
      i -= n;
      return *this;
    }
    constexpr iterator operator-(difference_type n) const {
      iterator ret = *this;
      ret -= n;
      return ret;
    }
    constexpr difference_type operator-(const iterator & r) const {
      return i - r.i;
    }

    constexpr bool operator==(const iterator & r) const noexcept {
      return i == r.i;
    }
    constexpr bool operator!=(const iterator & r) const noexcept {
      return !(*this == r);
    }
    constexpr bool operator<(const iterator & r) const noexcept {
      return i < r.i;
    }
    constexpr bool operator>(const iterator & r) const noexcept {
      return r < *this;
    }
    constexpr bool operator<=(const iterator & r) const noexcept {
      return !(*this > r);
    }
    constexpr bool operator>=(const iterator & r) const noexcept {
      return !(*this < r);
    }

  private:
    I i;
  };

  iota_view() = default;
  constexpr iota_view(I b, I e) : b(b), e(e) {}

  constexpr iterator begin() const noexcept {
    return b;
  }
  constexpr iterator end() const noexcept {
    return e;
  }

  constexpr bool empty() const {
    return b == e;
  }
  constexpr explicit operator bool() const {
    return !empty();
  }

  constexpr auto size() const {
    return e - b;
  }

  constexpr decltype(auto) front() const {
    return *begin();
  }
  constexpr decltype(auto) back() const {
    return *--end();
  }
  constexpr decltype(auto) operator[](I i) const {
    return begin()[i];
  }

private:
  iterator b, e;
};

template<class C>
struct index_iterator {
private:
  C * c;
  std::size_t i;

public:
  using reference = decltype((*c)[i]);
  using value_type = std::remove_reference_t<reference>;
  using pointer =
    std::conditional_t<std::is_reference_v<reference>, value_type *, void>;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::random_access_iterator_tag;

  index_iterator() noexcept : index_iterator(nullptr, 0) {}
  index_iterator(C * p, std::size_t i) : c(p), i(i) {}

  decltype(auto) operator*() const {
    return (*this)[0];
  }
  auto operator-> () const {
    return &**this;
  }

  decltype(auto) operator[](difference_type n) const {
    return (*c)[i + n];
  }

  index_iterator & operator++() {
    ++i;
    return *this;
  }
  index_iterator operator++(int) {
    index_iterator ret = *this;
    ++*this;
    return ret;
  }
  index_iterator & operator--() {
    --i;
    return *this;
  }
  index_iterator operator--(int) {
    index_iterator ret = *this;
    --*this;
    return ret;
  }

  index_iterator & operator+=(difference_type n) {
    i += n;
    return *this;
  }
  friend index_iterator operator+(difference_type n, index_iterator i) {
    return i += n;
  }
  index_iterator operator+(difference_type n) const {
    return n + *this;
  }
  index_iterator & operator-=(difference_type n) {
    i -= n;
    return *this;
  }
  friend index_iterator operator-(difference_type n, index_iterator i) {
    return i -= n;
  }
  index_iterator operator-(difference_type n) const {
    return n - *this;
  }
  difference_type operator-(const index_iterator & o) const {
    return i - o.i;
  }

  bool operator==(const index_iterator & o) const {
    return i == o.i;
  }
  bool operator!=(const index_iterator & o) const {
    return i != o.i;
  }
  bool operator<(const index_iterator & o) const {
    return i < o.i;
  }
  bool operator<=(const index_iterator & o) const {
    return i <= o.i;
  }
  bool operator>(const index_iterator & o) const {
    return i > o.i;
  }
  bool operator>=(const index_iterator & o) const {
    return i >= o.i;
  }

  std::size_t index() const noexcept {
    return i;
  }
};

template<class D> // CRTP, but D might be const
struct with_index_iterator {
  using iterator = index_iterator<D>;

  iterator begin() const noexcept {
    return {derived(), 0};
  }
  iterator end() const noexcept {
    const auto * p = derived();
    return {p, p->size()};
  }

private:
  D * derived() const noexcept {
    return static_cast<D *>(this);
  }
};

/// A very simple emulation of std::ranges::transform_view from C++20.
template<class I, class F>
struct transform_view {
  struct iterator {
  private:
    using traits = std::iterator_traits<I>;

  public:
    using difference_type = typename traits::difference_type;
    // TODO: notice a reference return from F and upgrade iterator_category
    using reference = decltype(
      std::declval<const F &>()(std::declval<typename traits::reference>()));
    using value_type = std::decay_t<reference>;
    using pointer = void;
    // We provide all the operators, but we don't assume a real reference:
    using iterator_category = std::input_iterator_tag;

    constexpr iterator() noexcept
      : iterator({}, nullptr) {} // null F won't be used
    constexpr iterator(I p, const F * f) noexcept : p(p), f(f) {}

    constexpr iterator & operator++() {
      ++p;
      return *this;
    }
    constexpr iterator operator++(int) {
      const iterator ret = *this;
      ++*this;
      return ret;
    }
    constexpr iterator & operator--() {
      --p;
      return *this;
    }
    constexpr iterator operator--(int) {
      const iterator ret = *this;
      --*this;
      return ret;
    }
    constexpr iterator & operator+=(difference_type n) {
      p += n;
      return *this;
    }
    friend constexpr iterator operator+(difference_type n, iterator i) {
      i += n;
      return i;
    }
    constexpr iterator operator+(difference_type n) const {
      return n + *this;
    }
    constexpr iterator & operator-=(difference_type n) {
      p -= n;
      return *this;
    }
    constexpr iterator operator-(difference_type n) const {
      iterator ret = *this;
      ret -= n;
      return ret;
    }
    constexpr difference_type operator-(const iterator & i) const {
      return p - i.p;
    }

    constexpr bool operator==(const iterator & i) const noexcept {
      return p == i.p;
    }
    constexpr bool operator!=(const iterator & i) const noexcept {
      return !(*this == i);
    }
    constexpr bool operator<(const iterator & i) const noexcept {
      return p < i.p;
    }
    constexpr bool operator>(const iterator & i) const noexcept {
      return i < *this;
    }
    constexpr bool operator<=(const iterator & i) const noexcept {
      return !(*this > i);
    }
    constexpr bool operator>=(const iterator & i) const noexcept {
      return !(*this < i);
    }

    FLECSI_INLINE_TARGET
    constexpr reference operator*() const {
      return (*f)(*p);
    }
    // operator-> makes sense only for a true 'reference'
    FLECSI_INLINE_TARGET
    constexpr reference operator[](difference_type n) const {
      return *(*this + n);
    }

  private:
    I p;
    const F * f;
  };

  /// Wrap an iterator pair.
  constexpr transform_view(I b, I e, F f = {})
    : b(std::move(b)), e(std::move(e)), f(std::move(f)) {}
  /// Wrap a container.
  /// \warning Destroying \a C invalidates this object if it owns its
  ///   iterators or elements.  This implementation does not copy \a C if it
  ///   is a view.
  template<class C,
    class = std::enable_if_t<
      std::is_convertible_v<decltype(std::begin(std::declval<C &>())), I>>>
  constexpr transform_view(C && c, F f = {})
    : transform_view(std::begin(c), std::end(c), std::move(f)) {}

  FLECSI_INLINE_TARGET
  constexpr iterator begin() const noexcept {
    return {b, &f};
  }
  FLECSI_INLINE_TARGET
  constexpr iterator end() const noexcept {
    return {e, &f};
  }

  FLECSI_INLINE_TARGET
  constexpr bool empty() const {
    return b == e;
  }
  FLECSI_INLINE_TARGET
  constexpr explicit operator bool() const {
    return !empty();
  }

  FLECSI_INLINE_TARGET
  constexpr auto size() const {
    return std::distance(b, e);
  }

  FLECSI_INLINE_TARGET
  constexpr decltype(auto) front() const {
    return *begin();
  }

  FLECSI_INLINE_TARGET
  constexpr decltype(auto) back() const {
    return *--end();
  }

  FLECSI_INLINE_TARGET
  constexpr decltype(auto) operator[](
    typename std::iterator_traits<I>::difference_type i) const {
    return begin()[i];
  }

private:
  I b, e;
  F f;
};

template<class C, class F>
transform_view(C &&, F)
  ->transform_view<typename std::remove_reference_t<C>::iterator, F>;
template<class C, class F>
transform_view(const C &, F)->transform_view<typename C::const_iterator, F>;

} // namespace util
} // namespace flecsi
