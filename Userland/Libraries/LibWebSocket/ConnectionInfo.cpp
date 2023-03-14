/*
 * Copyright (c) 2021, Dex♪ <dexes.ttp@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWebSocket/ConnectionInfo.h>

namespace WebSocket {

ConnectionInfo::ConnectionInfo(URL url)
    : m_url(move(url))
{
}

bool ConnectionInfo::is_secure() const
{
    // RFC 6455 Section 3 :
    // The URI is called "secure" if the scheme component matches "wss" case-insensitively.
    auto predicate = MUST(String::from_utf8("wss"sv));
    return m_url.scheme().equals_ignoring_case(predicate);
}

DeprecatedString ConnectionInfo::resource_name() const
{
    // RFC 6455 Section 3 :
    // The "resource-name" can be constructed by concatenating the following:
    StringBuilder builder;
    // "/" if the path component is empty
    auto path = MUST(m_url.path());
    if (path.is_empty())
        builder.append('/');
    // The path component
    builder.append(path);
    // "?" if the query component is non-empty
    if (!m_url.query().is_empty())
        builder.append('?');
    // the query component
    builder.append(m_url.query());
    return builder.to_deprecated_string();
}

}
