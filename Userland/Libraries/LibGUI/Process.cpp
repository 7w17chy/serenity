/*
 * Copyright (c) 2022, MacDue <macdue@dueutil.tech>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringView.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/Process.h>

template<typename StringType>
void spawn_or_show_error(GUI::Window* parent_window, StringView path, ReadonlySpan<StringType> arguments)
{
    auto spawn_result = Core::Process::spawn(path, arguments);
    if (spawn_result.is_error())
        GUI::MessageBox::show_error(parent_window, DeprecatedString::formatted("Failed to spawn {}: {}", path, spawn_result.error()));
}

namespace GUI {

void Process::spawn_or_show_error(Window* parent_window, StringView path, ReadonlySpan<DeprecatedString> arguments)
{
    ::spawn_or_show_error<DeprecatedString>(parent_window, path, arguments);
}

void Process::spawn_or_show_error(Window* parent_window, StringView path, ReadonlySpan<StringView> arguments)
{
    ::spawn_or_show_error<StringView>(parent_window, path, arguments);
}

void Process::spawn_or_show_error(Window* parent_window, StringView path, ReadonlySpan<char const*> arguments)
{
    ::spawn_or_show_error<char const*>(parent_window, path, arguments);
}

}
