/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Layout/CheckBox.h>
#include <LibWeb/Painting/LabelablePaintable.h>

namespace Web::Painting {

class CheckBoxPaintable final : public LabelablePaintable {
    JS_CELL(CheckBoxPaintable, LabelablePaintable);

public:
    static JS::NonnullGCPtr<CheckBoxPaintable> create(Layout::CheckBox const&);

    Layout::CheckBox const& layout_box() const;
    Layout::CheckBox& layout_box();

private:
    CheckBoxPaintable(Layout::CheckBox const&);
};

}
