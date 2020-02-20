// File ./ropto/ByteStream.hpp
//
//  ByteStream.hpp
//  ropto
//
//  Created by Kimmy  on 11/7/15.
//  Copyright (c) 2015 TDCA. All rights reserved.
//

#ifndef ropto_ByteStream_hpp
#define ropto_ByteStream_hpp

#include <utility>
#include <vector>
#include <algorithm>

namespace ropto {

    typedef std::vector<uint8_t> bytes_t;

    class byte_stream {
        bytes_t bytes{};
        int index = 0;

    public:
        byte_stream() = default;;

        explicit byte_stream(bytes_t _bytes) : bytes(std::move(_bytes)) {}

        byte_stream(const byte_stream &stream) = delete;

        void append(uint8_t byte) {
            bytes.push_back(byte);
        }

        uint8_t fetch() {
            return bytes[index++];
        }

        bytes_t &iterate() {
            return bytes;
        }

        auto begin() -> decltype(bytes.begin()) const {
            return std::begin(bytes);
        }

        auto end() -> decltype(bytes.end()) const {
            return std::end(bytes);
        }

        [[nodiscard]] size_t size() const {
            return bytes.size();
        }

        [[nodiscard]] bool fit(size_t size) const {
            return index + size < this->size();
        }

        template<class T>
        [[nodiscard]] bool fit() const {
            return fit(sizeof(T));
        }
    };
}

#endif
// File ./ropto/Serializer.hpp

#ifndef ropto_Serializer_hpp
#define ropto_Serializer_hpp

#include <type_traits>

namespace ropto {

    template<class T, bool pod = std::is_pod<T>::value,
            bool integral = std::is_integral<T>::value,
            bool floating = std::is_floating_point<T>::value>
    class serializer;

    template<class T>
    T read(byte_stream &stream) {
        return serializer<T>::from_bytes(stream);
    }

    template<class T>
    void write(const T &value, byte_stream &stream) {
        serializer<T>::to_bytes(value, stream);
    }

    template<class T>
    void read(T &value, byte_stream &stream) {
        value = read<T>(stream);
    }

    template<class... Args>
    void write(byte_stream &stream, Args &... args) {
        (write(args, stream), ...);
    }

    template<class... Args>
    void read(byte_stream &stream, Args &... args) {
        (read(args, stream), ...);
    }

    template<class T>
    byte_stream &operator<<(byte_stream &stream, const T &value) {
        write(value, stream);
        return stream;
    }

    template<class T>
    byte_stream &operator>>(byte_stream &stream, T &value) {
        read(stream, value);
        return stream;
    }
}

#endif
// File ./ropto/BinaryRetriver.hpp

#ifndef ropto_BinaryRetriver_hpp
#define ropto_BinaryRetriver_hpp

#include <cstdint>

namespace ropto {

    template<class T>
    union binary_retriver {
    public:
        T value;
        uint8_t bytes[sizeof(T)]{};

        binary_retriver() = default;

        explicit binary_retriver(const T &_value) : value(_value) {}
    };
}

#endif
// File ./ropto/Optional.hpp

#ifndef ropto_Optional_hpp
#define ropto_Optional_hpp

#include <type_traits>

// #if __has_include(<optional>)
// #include <optional>
// namespace ropto
// {
// template <class T>
// class serializer<std::optional<T>>
// {
//   public:
//     static std::optional<T> from_bytes(byte_stream &stream)
//     {
//         T value;
//         bool valid = false;
//         read(stream, valid);
//         if (valid)
//             read(stream, value);
//         return valid ? optional<T>{value} : optional<T>{};
//     }

//     static void to_bytes(const std::optional<T> &value, byte_stream &stream)
//     {
//         write(static_cast<bool>(value), stream);
//         if (value)
//             write(*value, stream);
//     }
// };
// } // namespace ropto
// #else  // !__has_include(<include>)

namespace ropto {
    template<class T>
    class optional {
    private:
        T __value;
        bool _has_value;

    public:
        optional() : __value{}, _has_value{false} {}

        explicit optional(const T &value) : __value{value}, _has_value{true} {}

        explicit operator bool() const {
            return _has_value;
        }

        optional<T> &operator=(const T &value) {
            _has_value = true;
            __value = value;
        }

        optional(const optional<T> &value) {
            _has_value = value._has_value;
            if (_has_value)
                __value = value.__value;
        }

        optional<T> &operator=(optional<T> value) {
            swap(value);
            return *this;
        }

        void swap(optional<T> &value) {
            std::swap(_has_value, value._has_value);
            std::swap(__value, value.__value);
        }

        T value() const {
            return __value;
        }

        T &value_or(const T &defaults) const {
            return _has_value ? __value : defaults;
        }
    };

    template<class T>
    class serializer<optional<T>, false, false, false> {
    public:
        static optional<T> from_bytes(byte_stream &stream) {
            T value;
            bool valid = false;
            read(stream, valid);
            if (valid)
                read(stream, value);
            return valid ? optional<T>{value} : optional<T>{};
        }

        static void to_bytes(const optional<T> &value, byte_stream &stream) {
            write(static_cast<bool>(value), stream);
            if (value)
                write(value.value(), stream);
        }
    };
}
// #endif // __has_include(<optional>)

#endif
// File ./ropto/SerializerSupport.hpp

#ifndef ropto_SerializerSupport_hpp
#define ropto_SerializerSupport_hpp

#include <iterator>
#include <map>

namespace ropto {
    template<class T>
    class serializer<T, true, false, false> {
    public:
        static T from_bytes(byte_stream &stream) {
            binary_retriver<T> retriver;
            for (int i = 0; i < sizeof(retriver); i++)
                retriver.bytes[i] = stream.fetch();
            return retriver.value;
        }

        static void to_bytes(const T &value, byte_stream &stream) {
            binary_retriver<T> retriver{value};
            for (auto byte : retriver.bytes)
                stream.append(byte);
        }
    };

    template<class Integral>
    class serializer<Integral, true, true, false> {
    public:
        static Integral from_bytes(byte_stream &stream) {
            std::decay_t<Integral> value = 0;
            for (size_t i = sizeof(Integral); i > 0; i--) {
                value += static_cast<Integral>(stream.fetch()) << (8 * (i - 1));
            }
            return value;
        }

        static void to_bytes(const Integral &value, byte_stream &stream) {
            for (size_t i = sizeof(Integral); i > 0; i--) {
                uint8_t byte = (value >> (8 * (i - 1))) & 0xFF;
                stream.append(byte);
            }
        }
    };

    template<class T, int size = sizeof(T)>
    union Integral;

    template<class T>
    union Integral<T, 1> {
        int8_t i;
        T t;
    };

    template<class T>
    union Integral<T, 2> {
        int16_t i;
        T t;
    };

    template<class T>
    union Integral<T, 4> {
        int32_t i;
        T t;
    };

    template<class T>
    union Integral<T, 8> {
        int64_t i;
        T t;
    };

    template<class T>
    union Integral<T, 16> {
        __int128_t i;
        T t;
    };

    template<class Floating>
    class serializer<Floating, true, false, true> {
        typedef Integral<Floating> Int;

    public:
        static Floating from_bytes(byte_stream &stream) {
            Int value{};
            read(value.i, stream);
            return value.t;
        }

        static void to_bytes(const Floating &value, byte_stream &stream) {
            Int ivalue{};
            ivalue.t = value;
            write(ivalue.i, stream);
        }
    };

    template<class Container>
    struct OutIter {
        typedef std::true_type is_back_insertable;
        typedef std::back_insert_iterator<Container> type;
    };

    template<class K, class V>
    struct OutIter<std::map<K, V>> {
        typedef std::false_type is_back_insertable;
        typedef std::insert_iterator<std::map<K, V>> type;
    };

    template<class Container>
    using OutIter_t = typename OutIter<Container>::type;

    template<class Container>
    OutIter_t<Container> InserterOf(Container &container, std::false_type) {
        return std::inserter(container, end(container));
    }

    template<class Container>
    OutIter_t<Container> InserterOf(Container &container, std::true_type) {
        return std::back_inserter(container);
    }

    template<class Container>
    OutIter_t<Container> Inserter(Container &container) {
        return InserterOf(container, typename OutIter<Container>::is_back_insertable{});
    }

    template<class Container>
    class serializer<Container, false, false, false> {
    public:
        static Container from_bytes(byte_stream &stream) {
            Container container;
            auto count = read<size_t>(stream);
            std::generate_n(Inserter(container), count, [&stream]() {
                return read<typename Container::value_type>(stream);
            });

            return container;
        }

        static void to_bytes(const Container &container, byte_stream &stream) {
            write(container.size(), stream);
            for (auto item : container)
                write(item, stream);
        }
    };

    template<class K, class V>
    class serializer<std::pair<K, V>, false, false, false> {
    public:
        static std::pair<K, V> from_bytes(byte_stream &stream) {
            auto first = read<std::decay_t<K>>(stream);
            auto second = read<V>(stream);
            return std::make_pair(first, second);
        }

        static void to_bytes(const std::pair<K, V> &pair, byte_stream &stream) {
            write(pair.first, stream);
            write(pair.second, stream);
        }
    };
}

#endif // File ./ropto/Base64.hpp

#ifndef ropto_Base64_hpp
#define ropto_Base64_hpp

#include <string>
#include <vector>

namespace ropto {
    std::string base64_encode(const std::vector<uint8_t> &bytes);

    std::vector<uint8_t> base64_decode(const std::string &input);

    constexpr static char base64map[] = ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

    constexpr static char pad_char = ('=');

    inline std::string base64_encode(const std::vector<uint8_t> &bytes) {
        std::string encoded;
        encoded.reserve(((bytes.size() / 3) + (bytes.size() % 3 > 0)) * 4);
        uint32_t temp;
        auto cursor = bytes.begin();
        for (size_t idx = 0; idx < bytes.size() / 3; idx++) {
            temp = (*cursor++) << 16;
            temp += (*cursor++) << 8;
            temp += (*cursor++);
            encoded.append(1, base64map[(temp & 0x00FC0000) >> 18]);
            encoded.append(1, base64map[(temp & 0x0003F000) >> 12]);
            encoded.append(1, base64map[(temp & 0x00000FC0) >> 6]);
            encoded.append(1, base64map[(temp & 0x0000003F)]);
        }
        switch (bytes.size() % 3) {
            case 1:
                temp = (*cursor++) << 16;
                encoded.append(1, base64map[(temp & 0x00FC0000) >> 18]);
                encoded.append(1, base64map[(temp & 0x0003F000) >> 12]);
                encoded.append(2, pad_char);
                break;
            case 2:
                temp = (*cursor++) << 16;
                temp += (*cursor++) << 8;
                encoded.append(1, base64map[(temp & 0x00FC0000) >> 18]);
                encoded.append(1, base64map[(temp & 0x0003F000) >> 12]);
                encoded.append(1, base64map[(temp & 0x00000FC0) >> 6]);
                encoded.append(1, pad_char);
                break;
        }
        return encoded;
    }

    inline std::vector<uint8_t> base64_decode(const std::string &input) {
        if (input.length() % 4)
            throw std::runtime_error("Non-Valid base64!");
        size_t padding = 0;
        if (input.length()) {
            if (input[input.length() - 1] == pad_char)
                padding++;
            if (input[input.length() - 2] == pad_char)
                padding++;
        }

        std::vector<uint8_t> decoded;
        decoded.reserve(((input.length() / 4) * 3) - padding);
        uint32_t temp = 0;
        auto cursor = input.begin();
        while (cursor < input.end()) {
            for (size_t position = 0; position < 4; position++) {
                temp <<= 6;
                if (*cursor >= 0x41 && *cursor <= 0x5A)
                    temp |= *cursor - 0x41;
                else if (*cursor >= 0x61 && *cursor <= 0x7A)
                    temp |= *cursor - 0x47;
                else if (*cursor >= 0x30 && *cursor <= 0x39)
                    temp |= *cursor + 0x04;
                else if (*cursor == 0x2B)
                    temp |= 0x3E;
                else if (*cursor == 0x2F)
                    temp |= 0x3F;
                else if (*cursor == pad_char) {
                    switch (input.end() - cursor) {
                        case 1:
                            decoded.push_back((temp >> 16) & 0x000000FF);
                            decoded.push_back((temp >> 8) & 0x000000FF);
                            return decoded;
                        case 2:
                            decoded.push_back((temp >> 10) & 0x000000FF);
                            return decoded;
                        default:
                            throw std::runtime_error("Invalid Padding in Base 64!");
                    }
                } else
                    throw std::runtime_error("Non-Valid Character in Base 64!");
                cursor++;
            }
            decoded.push_back((temp >> 16) & 0x000000FF);
            decoded.push_back((temp >> 8) & 0x000000FF);
            decoded.push_back((temp) & 0x000000FF);
        }
        return decoded;
    }
}

#endif /* defined(__ropto__Base64__) */
// File ./ropto/Service.hpp
//
//  Service.h
//  ropto
//
//  Created by Kimmy  on 3/15/16.
//  Copyright (c) 2016 TDCA. All rights reserved.
//

#ifndef __ropto__Service__
#define __ropto__Service__

#include <memory>
#include <functional>
#include <cassert>

namespace ropto {

    struct message_buf {
        unsigned int type_id{};
        bytes_t bytes;

        [[nodiscard]] std::unique_ptr<byte_stream> stream() const {
            auto stream = std::make_unique<byte_stream>();
            stream->iterate() = bytes;
            return stream;
        }
    };

    template<class T>
    message_buf make_message(T &object) {
        byte_stream stream;
        write(stream, object);

        return message_buf{T::type_id, stream.iterate()};
    }

    template<>
    class serializer<message_buf> {
    public:
        static message_buf from_bytes(byte_stream &stream) {
            message_buf mb;
            read(stream, mb.type_id, mb.bytes);
            return mb;
        }

        static void to_bytes(const message_buf &mb, byte_stream &stream) {
            write(stream, mb.type_id, mb.bytes);
        }
    };

    template<class Tin, class Tout = Tin>
    class service {
        static constexpr unsigned int type_in = Tin::type_id;
        static constexpr unsigned int type_out = Tout::type_id;

        std::function<void(Tin &, Tout &)> handler;

    public:
        explicit service(std::function<void(Tin &, Tout &)> fn) : handler(fn) {};

        service(const service &) = delete;

        message_buf process(const message_buf &mb) {
            assert(mb.type_id == type_in);
            Tout out{};
            auto in = read<Tin>(*mb.stream());
            handler(in, out);

            return make_message(out);
        }
    };

    template<class Tin, class Tout = Tin>
    std::shared_ptr<service<Tin, Tout>> make_service(std::function<void(Tin &, Tout &)> fn) {
        return std::make_shared<service<Tin, Tout>>(fn);
    }
}

#endif /* defined(__ropto__Service__) */
// File ropto.hpp

#ifndef ropto_ropto_hpp
#define ropto_ropto_hpp

#endif
