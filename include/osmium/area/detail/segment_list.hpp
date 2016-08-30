#ifndef OSMIUM_AREA_DETAIL_SEGMENT_LIST_HPP
#define OSMIUM_AREA_DETAIL_SEGMENT_LIST_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2016 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <numeric>
#include <vector>

#include <osmium/area/problem_reporter.hpp>
#include <osmium/area/detail/node_ref_segment.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node_ref.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/way.hpp>

namespace osmium {

    namespace area {

        namespace detail {

            /**
             * Iterate over all relation members and the vector of ways at the
             * same time and call given function with the relation member and
             * way as parameter. This takes into account that there might be
             * non-way members in the relation.
             */
            template <typename F>
            inline void for_each_member(const osmium::Relation& relation, const std::vector<const osmium::Way*> ways, F&& func) {
                auto way_it = ways.cbegin();
                for (const osmium::RelationMember& member : relation.members()) {
                    if (member.type() == osmium::item_type::way) {
                        assert(way_it != ways.cend());
                        func(member, **way_it);
                        ++way_it;
                    }
                }
            }

            /**
             * This is a helper class for the area assembler. It models
             * a list of segments.
             */
            class SegmentList {

                using slist_type = std::vector<NodeRefSegment>;

                slist_type m_segments;

                bool m_debug;

                static role_type parse_role(const char* role) noexcept {
                    if (role[0] == '\0') {
                        return role_type::empty;
                    } else if (!std::strcmp(role, "outer")) {
                        return role_type::outer;
                    } else if (!std::strcmp(role, "inner")) {
                        return role_type::inner;
                    }
                    return role_type::unknown;
                }

                /**
                 * Calculate the number of segments in all the ways together.
                 */
                static size_t get_num_segments(const std::vector<const osmium::Way*>& members) noexcept {
                    return std::accumulate(members.cbegin(), members.cend(), 0, [](size_t sum, const osmium::Way* way) {
                        if (way->nodes().empty()) {
                            return sum;
                        } else {
                            return sum + way->nodes().size() - 1;
                        }
                    });
                }

                uint32_t extract_segments_from_way_impl(osmium::area::ProblemReporter* problem_reporter, const osmium::Way& way, role_type role) {
                    uint32_t duplicate_nodes = 0;

                    osmium::NodeRef previous_nr;
                    for (const osmium::NodeRef& nr : way.nodes()) {
                        if (previous_nr.location()) {
                            if (previous_nr.location() != nr.location()) {
                                m_segments.emplace_back(previous_nr, nr, role, &way);
                            } else {
                                ++duplicate_nodes;
                                if (problem_reporter) {
                                    problem_reporter->report_duplicate_node(previous_nr.ref(), nr.ref(), nr.location());
                                }
                            }
                        }
                        previous_nr = nr;
                    }

                    return duplicate_nodes;
                }

            public:

                explicit SegmentList(bool debug) noexcept :
                    m_segments(),
                    m_debug(debug) {
                }

                ~SegmentList() noexcept = default;

                SegmentList(const SegmentList&) = delete;
                SegmentList(SegmentList&&) = delete;

                SegmentList& operator=(const SegmentList&) = delete;
                SegmentList& operator=(SegmentList&&) = delete;

                /// The number of segments in the list.
                size_t size() const noexcept {
                    return m_segments.size();
                }

                /// Is the segment list empty?
                bool empty() const noexcept {
                    return m_segments.empty();
                }

                using const_iterator = slist_type::const_iterator;
                using iterator = slist_type::iterator;

                NodeRefSegment& front() {
                    return m_segments.front();
                }

                NodeRefSegment& back() {
                    return m_segments.back();
                }

                const NodeRefSegment& operator[](size_t n) const noexcept {
                    assert(n < m_segments.size());
                    return m_segments[n];
                }

                NodeRefSegment& operator[](size_t n) noexcept {
                    assert(n < m_segments.size());
                    return m_segments[n];
                }

                iterator begin() noexcept {
                    return m_segments.begin();
                }

                iterator end() noexcept {
                    return m_segments.end();
                }

                const_iterator begin() const noexcept {
                    return m_segments.begin();
                }

                const_iterator end() const noexcept {
                    return m_segments.end();
                }

                /**
                 * Enable or disable debug output to stderr. This is used
                 * for debugging libosmium itself.
                 */
                void enable_debug_output(bool debug = true) noexcept {
                    m_debug = debug;
                }

                /// Sort the list of segments.
                void sort() {
                    std::sort(m_segments.begin(), m_segments.end());
                }

                /**
                 * Extract segments from given way and add them to the list.
                 *
                 * Segments connecting two nodes with the same location (ie
                 * same node or different nodes with same location) are
                 * removed after reporting the duplicate node.
                 */
                uint32_t extract_segments_from_way(osmium::area::ProblemReporter* problem_reporter, const osmium::Way& way) {
                    if (way.nodes().empty()) {
                        return 0;
                    }
                    m_segments.reserve(way.nodes().size() - 1);
                    return extract_segments_from_way_impl(problem_reporter, way, role_type::outer);
                }

                /**
                 * Extract all segments from all ways that make up this
                 * multipolygon relation and add them to the list.
                 */
                uint32_t extract_segments_from_ways(osmium::area::ProblemReporter* problem_reporter, const osmium::Relation& relation, const std::vector<const osmium::Way*>& members) {
                    assert(relation.members().size() >= members.size());

                    size_t num_segments = get_num_segments(members);
                    if (problem_reporter) {
                        problem_reporter->set_nodes(num_segments);
                    }
                    m_segments.reserve(num_segments);

                    uint32_t duplicate_nodes = 0;
                    for_each_member(relation, members, [this, &problem_reporter, &duplicate_nodes](const osmium::RelationMember& member, const osmium::Way& way) {
                        duplicate_nodes += extract_segments_from_way_impl(problem_reporter, way, parse_role(member.role()));
                    });

                    return duplicate_nodes;
                }

                /**
                 * Find duplicate segments (ie same start and end point) in the
                 * list and remove them. This will always remove pairs of the
                 * same segment. So if there are three, for instance, two will
                 * be removed and one will be left.
                 */
                uint32_t erase_duplicate_segments(osmium::area::ProblemReporter* problem_reporter) {
                    uint32_t duplicate_segments = 0;

                    while (true) {
                        auto it = std::adjacent_find(m_segments.begin(), m_segments.end());
                        if (it == m_segments.end()) {
                            break;
                        }
                        if (m_debug) {
                            std::cerr << "  erase duplicate segment: " << *it << "\n";
                        }

                        // Only count and report duplicate segments if they
                        // belong to the same way. Those cases are definitely
                        // wrong. If the duplicate segments belong to
                        // different ways, they could be touching inner rings
                        // which are perfectly okay.
                        if (it->way() == std::next(it)->way()) {
                            ++duplicate_segments;
                            if (problem_reporter) {
                                problem_reporter->report_duplicate_segment(it->first(), it->second());
                            }
                        }
                        m_segments.erase(it, it+2);
                    }

                    return duplicate_segments;
                }

                /**
                 * Find intersection between segments.
                 *
                 * @param problem_reporter Any intersections found are
                 *                         reported to this object.
                 * @returns true if there are intersections.
                 */
                uint32_t find_intersections(osmium::area::ProblemReporter* problem_reporter) const {
                    if (m_segments.empty()) {
                        return 0;
                    }

                    uint32_t found_intersections = 0;

                    for (auto it1 = m_segments.cbegin(); it1 != m_segments.cend()-1; ++it1) {
                        const NodeRefSegment& s1 = *it1;
                        for (auto it2 = it1+1; it2 != m_segments.end(); ++it2) {
                            const NodeRefSegment& s2 = *it2;

                            assert(s1 != s2); // erase_duplicate_segments() should have made sure of that

                            if (outside_x_range(s2, s1)) {
                                break;
                            }

                            if (y_range_overlap(s1, s2)) {
                                osmium::Location intersection = calculate_intersection(s1, s2);
                                if (intersection) {
                                    ++found_intersections;
                                    if (m_debug) {
                                        std::cerr << "  segments " << s1 << " and " << s2 << " intersecting at " << intersection << "\n";
                                    }
                                    if (problem_reporter) {
                                        problem_reporter->report_intersection(s1.way()->id(), s1.first().location(), s1.second().location(),
                                                                              s2.way()->id(), s2.first().location(), s2.second().location(), intersection);
                                    }
                                }
                            }
                        }
                    }

                    return found_intersections;
                }

            }; // class SegmentList

        } // namespace detail

    } // namespace area

} // namespace osmium

#endif // OSMIUM_AREA_DETAIL_SEGMENT_LIST_HPP
