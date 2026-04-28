#pragma once
#include <QObject>
#include "../../gt-stacer-core/Tools/service_tool.h"
#include "../../gt-stacer-core/Tools/package_tool.h"
#include "../../gt-stacer-core/Tools/startup_tool.h"
#include "../../gt-stacer-core/Tools/apt_source_tool.h"

// All Tool classes are pure-static utilities — no instances needed.
// ToolManager only provides a convenient access point.
class ToolManager : public QObject {
    Q_OBJECT
public:
    static ToolManager *instance();

private:
    explicit ToolManager(QObject *parent = nullptr);
    static ToolManager *m_instance;
};
