#include "tool_manager.h"

ToolManager *ToolManager::m_instance = nullptr;

ToolManager *ToolManager::instance()
{
    if (!m_instance) m_instance = new ToolManager;
    return m_instance;
}

ToolManager::ToolManager(QObject *parent) : QObject(parent) {}
