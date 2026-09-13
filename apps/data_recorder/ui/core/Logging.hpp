#ifndef APPS_DATA_RECORDER_UI_CORE_LOGGING_HPP
#define APPS_DATA_RECORDER_UI_CORE_LOGGING_HPP

#include <QLoggingCategory>

// General application lifecycle: configuration, stream setup, recording state.
Q_DECLARE_LOGGING_CATEGORY(uiLog)

// Per-frame tracing. Disabled by default because it fires at the frame rate of
// every stream; enable with QT_LOGGING_RULES="data_recorder.ui.frame.debug=true".
Q_DECLARE_LOGGING_CATEGORY(uiFrameLog)

#endif // APPS_DATA_RECORDER_UI_CORE_LOGGING_HPP
