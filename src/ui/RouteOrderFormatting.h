#pragma once

#include <cstddef>

#include "route/RouteOrderEditor.h"

namespace ui {

const char* routeOrderEditorTitle(const route::RouteOrderEditorView& editor);
const char* competitionTypeLabel(domain::CompetitionType competitionType);
void formatEditableRouteOrderValue(
    char* text, std::size_t size,
    const route::RouteOrderEditorView& editor);

}  // namespace ui
