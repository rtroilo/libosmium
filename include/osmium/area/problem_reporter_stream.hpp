#ifndef OSMIUM_AREA_PROBLEM_REPORTER_STREAM_HPP
#define OSMIUM_AREA_PROBLEM_REPORTER_STREAM_HPP

/*

This file is part of Osmium (https://osmcode.org/libosmium).

Copyright 2013-2018 Jochen Topf <jochen@topf.org> and others (see README).

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

#include <osmium/area/problem_reporter.hpp>
#include <osmium/osm/item_type.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node_ref.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/osm/way.hpp>

#include <ostream>

namespace osmium {

    namespace area {

        class ProblemReporterStream : public ProblemReporter {

            std::ostream* m_out;

        public:

            explicit ProblemReporterStream(std::ostream& out) :
                m_out(&out) {
            }

            void header(const char* msg) {
                *m_out << "DATA PROBLEM: " << msg << " on " << item_type_to_char(m_object_type) << m_object_id << " (with " << m_nodes << " nodes): ";
            }

            void report_duplicate_node(osmium::object_id_type node_id1, osmium::object_id_type node_id2, osmium::Location location) override {
                header("duplicate node");
                *m_out << "node_id1=" << node_id1 << " node_id2=" << node_id2 << " location=" << location << "\n";
            }

            void report_touching_ring(osmium::object_id_type node_id, osmium::Location location) override {
                header("touching ring");
                *m_out << "node_id=" << node_id << " location=" << location << "\n";
            }

            void report_intersection(osmium::object_id_type way1_id, osmium::Location way1_seg_start, osmium::Location way1_seg_end,
                                     osmium::object_id_type way2_id, osmium::Location way2_seg_start, osmium::Location way2_seg_end, osmium::Location intersection) override {
                header("intersection");
                *m_out << "way1_id=" << way1_id << " way1_seg_start=" << way1_seg_start << " way1_seg_end=" << way1_seg_end
                       << " way2_id=" << way2_id << " way2_seg_start=" << way2_seg_start << " way2_seg_end=" << way2_seg_end << " intersection=" << intersection << "\n";
            }

            void report_duplicate_segment(const osmium::NodeRef& nr1, const osmium::NodeRef& nr2) override {
                header("duplicate segment");
                *m_out << "node_id1=" << nr1.ref() << " location1=" << nr1.location()
                       << " node_id2=" << nr2.ref() << " location2=" << nr2.location() << "\n";
            }

            void report_overlapping_segment(const osmium::NodeRef& nr1, const osmium::NodeRef& nr2) override {
                header("overlapping segment");
                *m_out << "node_id1=" << nr1.ref() << " location1=" << nr1.location()
                       << " node_id2=" << nr2.ref() << " location2=" << nr2.location() << "\n";
            }

            void report_ring_not_closed(const osmium::NodeRef& nr, const osmium::Way* way) override {
                header("ring not closed");
                *m_out << "node_id=" << nr.ref() << " location=" << nr.location();
                if (way) {
                    *m_out << " on way " << way->id();
                }
                *m_out << "\n";
            }

            void report_role_should_be_outer(osmium::object_id_type way_id, osmium::Location seg_start, osmium::Location seg_end) override {
                header("role should be outer");
                *m_out << "way_id=" << way_id << " seg_start=" << seg_start << " seg_end=" << seg_end << "\n";
            }

            void report_role_should_be_inner(osmium::object_id_type way_id, osmium::Location seg_start, osmium::Location seg_end) override {
                header("role should be inner");
                *m_out << "way_id=" << way_id << " seg_start=" << seg_start << " seg_end=" << seg_end << "\n";
            }

            void report_way_in_multiple_rings(const osmium::Way& way) override {
                header("way in multiple rings");
                *m_out << "way_id=" << way.id() << '\n';
            }

            void report_inner_with_same_tags(const osmium::Way& way) override {
                header("inner way with same tags as relation or outer");
                *m_out << "way_id=" << way.id() << '\n';
            }

            void report_invalid_location(osmium::object_id_type way_id, osmium::object_id_type node_id) override {
                header("invalid location");
                *m_out << "way_id=" << way_id << " node_id=" << node_id << '\n';
            }

            void report_duplicate_way(const osmium::Way& way) override {
                header("duplicate way");
                *m_out << "way_id=" << way.id() << '\n';
            }

        }; // class ProblemReporterStream

    } // namespace area

} // namespace osmium

#endif // OSMIUM_AREA_PROBLEM_REPORTER_STREAM_HPP
