/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Max Wipfli <mail@maxwipfli.ch>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Vector.h>


// On Linux distros that use mlibc `basename` is defined as a macro that expands to `__mlibc_gnu_basename` or `__mlibc_gnu_basename_c`, so we undefine it.
#if defined(AK_OS_LINUX) && defined(basename)
#    undef basename
#endif

namespace AK {

// NOTE: The member variables cannot contain any percent encoded sequences.
//       The URL parser automatically decodes those sequences and the serialize() method will re-encode them as necessary.
class URL {
    friend class URLParser;

public:
    enum class PercentEncodeSet {
        C0Control,
        Fragment,
        Query,
        SpecialQuery,
        Path,
        Userinfo,
        Component,
        ApplicationXWWWFormUrlencoded,
        EncodeURI
    };

    enum class ExcludeFragment {
        No,
        Yes
    };

    URL() = default;
    URL(StringView);
    URL(String const& string)
        : URL(string.bytes_as_string_view())
    {
    }

    bool is_valid() const { return m_valid; }

    String const& scheme() const { return m_scheme; }
    String const& username() const { return m_username; }
    String const& password() const { return m_password; }
    String const& host() const { return m_host; }
    Vector<String> const& paths() const { return m_paths; }
    String const& query() const { return m_query; }
    String const& fragment() const { return m_fragment; }
    Optional<u16> port() const { return m_port; }
    u16 port_or_default() const { return m_port.value_or(default_port_for_scheme(m_scheme)); }
    bool cannot_be_a_base_url() const { return m_cannot_be_a_base_url; }
    bool cannot_have_a_username_or_password_or_port() const { return m_host.is_empty() || m_cannot_be_a_base_url || m_scheme == "file"sv; }

    bool includes_credentials() const { return !m_username.is_empty() || !m_password.is_empty(); }
    bool is_special() const { return is_special_scheme(m_scheme); }

    void set_scheme(String);
    void set_username(String);
    void set_password(String);
    void set_host(String);
    void set_port(Optional<u16>);
    void set_paths(Vector<String>);
    void set_query(String);
    void set_fragment(String);
    void set_cannot_be_a_base_url(bool value) { m_cannot_be_a_base_url = value; }
    void append_path(String path) { m_paths.append(move(path)); }

    ErrorOr<String> path() const;
    ErrorOr<String> basename() const;

    ErrorOr<String> serialize(ExcludeFragment = ExcludeFragment::No) const;
    ErrorOr<String> serialize_for_display() const;
    ErrorOr<String> to_string() const { return serialize(); }
    DeprecatedString to_deprecated_string() const;

    // HTML origin
    ErrorOr<String> serialize_origin() const;

    bool equals(URL const& other, ExcludeFragment = ExcludeFragment::No) const;

    ErrorOr<URL> complete_url(StringView) const;

    bool data_payload_is_base64() const { return m_data_payload_is_base64; }
    String const& data_mime_type() const { return m_data_mime_type; }
    String const& data_payload() const { return m_data_payload; }

    static ErrorOr<URL> create_with_url_or_path(String const&);
    static ErrorOr<URL> create_with_file_scheme(String const& path, String const& fragment = {}, String const& hostname = {});
    static ErrorOr<URL> create_with_help_scheme(String const& path, String const& fragment = {}, String const& hostname = {});
    static ErrorOr<URL> create_with_data(String mime_type, String payload, bool is_base64 = false);

    static bool scheme_requires_port(StringView);
    static u16 default_port_for_scheme(StringView);
    static bool is_special_scheme(StringView);

    enum class SpaceAsPlus {
        No,
        Yes,
    };
    static ErrorOr<String> percent_encode(StringView input, PercentEncodeSet set = PercentEncodeSet::Userinfo, SpaceAsPlus = SpaceAsPlus::No);
    static ErrorOr<String> percent_decode(StringView input);

    bool operator==(URL const& other) const { return equals(other, ExcludeFragment::No); }

    static bool code_point_is_in_percent_encode_set(u32 code_point, URL::PercentEncodeSet);

private:
    URL(String&& data_mime_type, String&& data_payload, String&& scheme, bool payload_is_base64)
        : m_valid(true)
        , m_scheme(scheme)
        , m_data_payload_is_base64(payload_is_base64)
        , m_data_mime_type(move(data_mime_type))
        , m_data_payload(move(data_payload))
    {
    }

    bool compute_validity() const;
    ErrorOr<String> serialize_data_url() const;

    static void append_percent_encoded_if_necessary(StringBuilder&, u32 code_point, PercentEncodeSet set = PercentEncodeSet::Userinfo);
    static void append_percent_encoded(StringBuilder&, u32 code_point);

    bool m_valid { false };

    String m_scheme;
    String m_username;
    String m_password;
    String m_host;
    // NOTE: If the port is the default port for the scheme, m_port should be empty.
    Optional<u16> m_port;
    String m_path;
    Vector<String> m_paths;
    String m_query;
    String m_fragment;

    bool m_cannot_be_a_base_url { false };

    bool m_data_payload_is_base64 { false };
    String m_data_mime_type;
    String m_data_payload;
};

template<>
struct Formatter<URL> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, URL const& value)
    {
        return Formatter<StringView>::format(builder, TRY(value.serialize()));
    }
};

template<>
struct Traits<URL> : public GenericTraits<URL> {
    static unsigned hash(URL const& url) { return url.to_deprecated_string().hash(); }
};

}
