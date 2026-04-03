// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2018 Barend Gehrels, Amsterdam, the Netherlands.

// Modified for Boost 1.89+ compatibility - uses new graph-based traverse

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_EXTENSIONS3_ALGORITHMS_DISSOLVE_TRAVERSE_HPP
#define BOOST_GEOMETRY_EXTENSIONS3_ALGORITHMS_DISSOLVE_TRAVERSE_HPP

#include <boost/geometry/algorithms/detail/overlay/traverse.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace dissolve
{


/*!
    \brief Traverses through intersection points / geometries for dissolve.
        Delegates to the standard graph-based traverse algorithm in Boost 1.89+.
    \ingroup dissolve
 */
template <bool Reverse>
class traverse
{

public :
    template
    <
        typename Geometry,
        typename IntersectionStrategy,
        typename Turns,
        typename Rings,
        typename TurnInfoMap,
        typename Clusters,
        typename Visitor
    >
    static inline void apply(Geometry const& geometry,
                IntersectionStrategy const& intersection_strategy,
                Turns& turns, Rings& rings,
                TurnInfoMap& turn_info_map,
                Clusters& clusters,
                Visitor& visitor)
    {
        detail::overlay::traverse
            <
                Reverse, Reverse,
                Geometry, Geometry,
                overlay_dissolve
            >::apply(geometry, geometry, intersection_strategy,
                     turns, rings, turn_info_map, clusters, visitor);
    }
};

}} // namespace detail::dissolve
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_EXTENSIONS3_ALGORITHMS_DISSOLVE_TRAVERSE_HPP
