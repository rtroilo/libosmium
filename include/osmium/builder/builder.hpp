#ifndef OSMIUM_BUILDER_BUILDER_HPP
#define OSMIUM_BUILDER_BUILDER_HPP

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
#include <new>
#include <string>
#include <type_traits>

#include <osmium/memory/buffer.hpp>
#include <osmium/memory/item.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/util/cast.hpp>

namespace osmium {

    /**
     * @brief Classes for building OSM objects and other items in buffers
     */
    namespace builder {

        class Builder {

            osmium::memory::Buffer& m_buffer;
            Builder* m_parent;
            size_t m_item_offset;

            Builder(const Builder&) = delete;
            Builder(Builder&&) = delete;

            Builder& operator=(const Builder&) = delete;
            Builder& operator=(Builder&&) = delete;

        protected:

            explicit Builder(osmium::memory::Buffer& buffer, Builder* parent, osmium::memory::item_size_type size) :
                m_buffer(buffer),
                m_parent(parent),
                m_item_offset(buffer.written()) {
                m_buffer.reserve_space(size);
                assert(buffer.is_aligned());
                if (m_parent) {
                    m_parent->add_size(size);
                }
            }

            ~Builder() = default;

            osmium::memory::Item& item() const {
                return *reinterpret_cast<osmium::memory::Item*>(m_buffer.data() + m_item_offset);
            }

        public:

            /**
             * Add padding to buffer (if needed) to align data properly.
             *
             * This calculates how many padding bytes are needed and adds
             * as many zero bytes to the buffer. It also adds this number
             * to the size of the current item (if the "self" param is
             * true) and recursively to all the parent items.
             *
             * @param self If true add number of padding bytes to size
             *             of current item. Size is always added to
             *             parent item (if any).
             *
             */
            void add_padding(bool self = false) {
                auto padding = osmium::memory::align_bytes - (size() % osmium::memory::align_bytes);
                if (padding != osmium::memory::align_bytes) {
                    std::fill_n(m_buffer.reserve_space(padding), padding, 0);
                    if (self) {
                        add_size(padding);
                    } else if (m_parent) {
                        m_parent->add_size(padding);
                        assert(m_parent->size() % osmium::memory::align_bytes == 0);
                    }
                }
            }

            void add_size(uint32_t size) {
                item().add_size(size);
                if (m_parent) {
                    m_parent->add_size(size);
                }
            }

            uint32_t size() const noexcept {
                return item().byte_size();
            }

            void add_item(const osmium::memory::Item* item) {
                unsigned char* target = m_buffer.reserve_space(item->padded_size());
                std::copy_n(reinterpret_cast<const unsigned char*>(item), item->padded_size(), target);
                add_size(item->padded_size());
            }

            /**
             * Reserve space for an object of class T in buffer and return
             * pointer to it.
             */
            template <typename T>
            T* reserve_space_for() {
                assert(m_buffer.is_aligned());
                return reinterpret_cast<T*>(m_buffer.reserve_space(sizeof(T)));
            }

            /**
             * Append data to buffer.
             *
             * @param data Pointer to data.
             * @param length Length of data in bytes. If data is a
             *               \0-terminated string, length must contain the
             *               \0 byte.
             * @returns The number of bytes appended (length).
             */
            osmium::memory::item_size_type append(const char* data, const osmium::memory::item_size_type length) {
                unsigned char* target = m_buffer.reserve_space(length);
                std::copy_n(reinterpret_cast<const unsigned char*>(data), length, target);
                return length;
            }

            /**
             * Append \0-terminated string to buffer.
             *
             * @param str \0-terminated string.
             * @returns The number of bytes appended (strlen(str) + 1).
             */
            osmium::memory::item_size_type append(const char* str) {
                return append(str, static_cast<osmium::memory::item_size_type>(std::strlen(str) + 1));
            }

            /**
             * Append '\0' to the buffer.
             *
             * @returns The number of bytes appended (always 1).
             */
            osmium::memory::item_size_type append_zero() {
                *m_buffer.reserve_space(1) = '\0';
                return 1;
            }

            /// Return the buffer this builder is using.
            osmium::memory::Buffer& buffer() noexcept {
                return m_buffer;
            }

        }; // class Builder

        template <typename TItem>
        class ObjectBuilder : public Builder {

            static_assert(std::is_base_of<osmium::memory::Item, TItem>::value, "ObjectBuilder can only build objects derived from osmium::memory::Item");

        public:

            explicit ObjectBuilder(osmium::memory::Buffer& buffer, Builder* parent = nullptr) :
                Builder(buffer, parent, sizeof(TItem)) {
                new (&item()) TItem();
            }

            TItem& object() noexcept {
                return static_cast<TItem&>(item());
            }

            /**
             * Add user name to buffer.
             *
             * @param user Pointer to user name.
             * @param length Length of user name (without \0 termination).
             */
            void add_user(const char* user, const string_size_type length) {
                object().set_user_size(length + 1);
                add_size(append(user, length) + append_zero());
                add_padding(true);
            }

            /**
             * Add user name to buffer.
             *
             * @param user Pointer to \0-terminated user name.
             */
            void add_user(const char* user) {
                add_user(user, static_cast_with_assert<string_size_type>(std::strlen(user)));
            }

            /**
             * Add user name to buffer.
             *
             * @param user User name.
             */
            void add_user(const std::string& user) {
                add_user(user.data(), static_cast_with_assert<string_size_type>(user.size()));
            }

        }; // class ObjectBuilder

    } // namespace builder

} // namespace osmium

#endif // OSMIUM_BUILDER_BUILDER_HPP
