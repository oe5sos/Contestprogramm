#pragma once

namespace Contestprogramm {

// Brings this process's application to the front. On macOS a background
// application cannot take focus through Qt alone (activateWindow() only
// makes the window key inside an already-active application), so the
// implementation there asks AppKit; elsewhere it is a no-op and
// QWidget::activateWindow() does the work. Used by the single-instance
// hand-over (app/SingleInstanceGuard.h).
void activateThisApplication();

} // namespace Contestprogramm
