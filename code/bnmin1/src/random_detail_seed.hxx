/* boost random/detail/seed.hpp header file
 *
 * Copyright Steven Watanabe 2009
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org for most recent version including documentation.
 *
 * $Id$
 */

#ifndef _BNMIN1_RANDOM_DETAIL_SEED_HXX_
#define _BNMIN1_RANDOM_DETAIL_SEED_HXX_

#include <boost/config.hpp>

#if !defined(BOOST_NO_SFINAE)

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_arithmetic.hpp>

namespace bnmin1boost {
namespace random {
namespace detail {

template<class T>
struct disable_seed : boost::disable_if<boost::is_arithmetic<T> > {};

template<class Engine, class T>
struct disable_constructor : disable_seed<T> {};

template<class Engine>
struct disable_constructor<Engine, Engine> {
};

#define BNMIN1_RANDOM_DETAIL_GENERATOR_CONSTRUCTOR(Self, Generator, gen) \
    template<class Generator>                                           \
    explicit Self(Generator& gen, typename ::bnmin1boost::random::detail::disable_constructor<Self, Generator>::type* = 0)

#define BNMIN1_RANDOM_DETAIL_GENERATOR_SEED(Self, Generator, gen)    \
    template<class Generator>                                       \
    void seed(Generator& gen, typename ::bnmin1boost::random::detail::disable_seed<Generator>::type* = 0)

#define BNMIN1_RANDOM_DETAIL_ARITHMETIC_CONSTRUCTOR(Self, T, x)  \
    explicit Self(const T& x)

#define BNMIN1_RANDOM_DETAIL_ARITHMETIC_SEED(Self, T, x) \
    void seed(const T& x)

}
}
}

#else

#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/mpl/bool.hpp>

#define BOOST_RANDOM_DETAIL_GENERATOR_CONSTRUCTOR(Self, Generator, gen) \
    Self(Self& other) { *this = other; }                                \
    Self(const Self& other) { *this = other; }                          \
    template<class Generator>                                           \
    explicit Self(Generator& gen) {                                     \
        boost_random_constructor_impl(gen, ::boost::is_arithmetic<Generator>());\
    }                                                                   \
    template<class Generator>                                           \
    void boost_random_constructor_impl(Generator& gen, ::boost::mpl::false_)

#define BOOST_RANDOM_DETAIL_GENERATOR_SEED(Self, Generator, gen)    \
    template<class Generator>                                       \
    void seed(Generator& gen) {                                     \
        boost_random_seed_impl(gen, ::boost::is_arithmetic<Generator>());\
    }\
    template<class Generator>\
    void boost_random_seed_impl(Generator& gen, ::boost::mpl::false_)

#define BOOST_RANDOM_DETAIL_ARITHMETIC_CONSTRUCTOR(Self, T, x)  \
    explicit Self(const T& x) { boost_random_constructor_impl(x, ::boost::mpl::true_()); }\
    void boost_random_constructor_impl(const T& x, ::boost::mpl::true_)

#define BOOST_RANDOM_DETAIL_ARITHMETIC_SEED(Self, T, x) \
    void seed(const T& x) { boost_random_seed_impl(x, ::boost::mpl::true_()); }\
    void boost_random_seed_impl(const T& x, ::boost::mpl::true_)

#endif

#endif
