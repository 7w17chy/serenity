/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Base64.h>
#include <AK/StringBuilder.h>
#include <LibHTTP/HttpRequest.h>
#include <LibHTTP/Job.h>

namespace HTTP {

DeprecatedString to_deprecated_string(HttpRequest::Method method)
{
    switch (method) {
    case HttpRequest::Method::GET:
        return "GET";
    case HttpRequest::Method::HEAD:
        return "HEAD";
    case HttpRequest::Method::POST:
        return "POST";
    case HttpRequest::Method::DELETE:
        return "DELETE";
    case HttpRequest::Method::PATCH:
        return "PATCH";
    case HttpRequest::Method::OPTIONS:
        return "OPTIONS";
    case HttpRequest::Method::TRACE:
        return "TRACE";
    case HttpRequest::Method::CONNECT:
        return "CONNECT";
    case HttpRequest::Method::PUT:
        return "PUT";
    default:
        VERIFY_NOT_REACHED();
    }
}

DeprecatedString HttpRequest::method_name() const
{
    return to_deprecated_string(m_method);
}

ErrorOr<ByteBuffer> HttpRequest::to_raw_request() const
{
    StringBuilder builder;
    TRY(builder.try_append(method_name()));
    TRY(builder.try_append(' '));
    // NOTE: The percent_encode is so that e.g. spaces are properly encoded.
    auto path = TRY(m_url.path());
    VERIFY(!path.is_empty());
    auto percent_encoded = TRY(URL::percent_encode(path, URL::PercentEncodeSet::EncodeURI));
    TRY(builder.try_append(percent_encoded));
    if (!m_url.query().is_empty()) {
        TRY(builder.try_append('?'));
        TRY(builder.try_append(m_url.query()));
    }
    TRY(builder.try_append(" HTTP/1.1\r\nHost: "sv));
    TRY(builder.try_append(m_url.host()));
    if (m_url.port().has_value())
        TRY(builder.try_appendff(":{}", *m_url.port()));
    TRY(builder.try_append("\r\n"sv));
    for (auto& header : m_headers) {
        TRY(builder.try_append(header.name));
        TRY(builder.try_append(": "sv));
        TRY(builder.try_append(header.value));
        TRY(builder.try_append("\r\n"sv));
    }
    if (!m_body.is_empty() || method() == Method::POST) {
        TRY(builder.try_appendff("Content-Length: {}\r\n\r\n", m_body.size()));
        TRY(builder.try_append((char const*)m_body.data(), m_body.size()));
    }
    TRY(builder.try_append("\r\n"sv));
    return builder.to_byte_buffer();
}

Optional<HttpRequest> HttpRequest::from_raw_request(ReadonlyBytes raw_request)
{
    enum class State {
        InMethod,
        InResource,
        InProtocol,
        InHeaderName,
        InHeaderValue,
        InBody,
    };

    State state { State::InMethod };
    size_t index = 0;

    auto peek = [&](int offset = 0) -> u8 {
        if (index + offset >= raw_request.size())
            return 0;
        return raw_request[index + offset];
    };

    auto consume = [&]() -> u8 {
        VERIFY(index < raw_request.size());
        return raw_request[index++];
    };

    Vector<u8, 256> buffer;

    DeprecatedString method;
    DeprecatedString resource;
    DeprecatedString protocol;
    Vector<Header> headers;
    Header current_header;
    ByteBuffer body;

    auto commit_and_advance_to = [&](auto& output, State new_state) {
        output = DeprecatedString::copy(buffer);
        buffer.clear();
        state = new_state;
    };

    while (index < raw_request.size()) {
        // FIXME: Figure out what the appropriate limitations should be.
        if (buffer.size() > 65536)
            return {};
        switch (state) {
        case State::InMethod:
            if (peek() == ' ') {
                consume();
                commit_and_advance_to(method, State::InResource);
                break;
            }
            buffer.append(consume());
            break;
        case State::InResource:
            if (peek() == ' ') {
                consume();
                commit_and_advance_to(resource, State::InProtocol);
                break;
            }
            buffer.append(consume());
            break;
        case State::InProtocol:
            if (peek(0) == '\r' && peek(1) == '\n') {
                consume();
                consume();
                commit_and_advance_to(protocol, State::InHeaderName);
                break;
            }
            buffer.append(consume());
            break;
        case State::InHeaderName:
            if (peek(0) == ':' && peek(1) == ' ') {
                consume();
                consume();
                commit_and_advance_to(current_header.name, State::InHeaderValue);
                break;
            }
            buffer.append(consume());
            break;
        case State::InHeaderValue:
            if (peek(0) == '\r' && peek(1) == '\n') {
                consume();
                consume();

                // Detect end of headers
                auto next_state = State::InHeaderName;
                if (peek(0) == '\r' && peek(1) == '\n') {
                    consume();
                    consume();
                    next_state = State::InBody;
                }

                commit_and_advance_to(current_header.value, next_state);
                headers.append(move(current_header));
                break;
            }
            buffer.append(consume());
            break;
        case State::InBody:
            buffer.append(consume());
            if (index == raw_request.size()) {
                // End of data, so store the body
                auto maybe_body = ByteBuffer::copy(buffer);
                // FIXME: Propagate this error somehow.
                if (maybe_body.is_error())
                    return {};
                body = maybe_body.release_value();
                buffer.clear();
            }
            break;
        }
    }

    HttpRequest request;
    if (method == "GET")
        request.m_method = Method::GET;
    else if (method == "HEAD")
        request.m_method = Method::HEAD;
    else if (method == "POST")
        request.m_method = Method::POST;
    else if (method == "DELETE")
        request.set_method(HTTP::HttpRequest::Method::DELETE);
    else if (method == "PATCH")
        request.set_method(HTTP::HttpRequest::Method::PATCH);
    else if (method == "OPTIONS")
        request.set_method(HTTP::HttpRequest::Method::OPTIONS);
    else if (method == "TRACE")
        request.set_method(HTTP::HttpRequest::Method::TRACE);
    else if (method == "CONNECT")
        request.set_method(HTTP::HttpRequest::Method::CONNECT);
    else if (method == "PUT")
        request.set_method(HTTP::HttpRequest::Method::PUT);
    else
        return {};

    request.m_headers = move(headers);
    auto url_parts = resource.split_limit('?', 2, SplitBehavior::KeepEmpty);

    request.m_url.set_cannot_be_a_base_url(true);
    if (url_parts.size() == 2) {
        request.m_resource = url_parts[0];
        request.m_url.set_paths({ MUST(String::from_utf8(url_parts[0])) });
        request.m_url.set_query(MUST(String::from_utf8(url_parts[1])));
    } else {
        request.m_resource = resource;
        request.m_url.set_paths({ MUST(String::from_utf8(resource)) });
    }

    request.set_body(move(body));

    return request;
}

void HttpRequest::set_headers(HashMap<DeprecatedString, DeprecatedString> const& headers)
{
    for (auto& it : headers)
        m_headers.append({ it.key, it.value });
}

Optional<HttpRequest::Header> HttpRequest::get_http_basic_authentication_header(URL const& url)
{
    if (!url.includes_credentials())
        return {};
    StringBuilder builder;
    builder.append(url.username());
    builder.append(':');
    builder.append(url.password());

    // FIXME: change to TRY() and make method fallible
    auto token = MUST(encode_base64(MUST(builder.to_string()).bytes()));
    builder.clear();
    builder.append("Basic "sv);
    builder.append(token);
    return Header { "Authorization", builder.to_deprecated_string() };
}

Optional<HttpRequest::BasicAuthenticationCredentials> HttpRequest::parse_http_basic_authentication_header(DeprecatedString const& value)
{
    if (!value.starts_with("Basic "sv, AK::CaseSensitivity::CaseInsensitive))
        return {};
    auto token = value.substring_view(6);
    if (token.is_empty())
        return {};
    auto decoded_token_bb = decode_base64(token);
    if (decoded_token_bb.is_error())
        return {};
    auto decoded_token = DeprecatedString::copy(decoded_token_bb.value());
    auto colon_index = decoded_token.find(':');
    if (!colon_index.has_value())
        return {};
    auto username = decoded_token.substring_view(0, colon_index.value());
    auto password = decoded_token.substring_view(colon_index.value() + 1);
    return BasicAuthenticationCredentials { username, password };
}

}
