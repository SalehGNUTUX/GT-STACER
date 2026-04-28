#include "info_manager.h"

InfoManager *InfoManager::m_instance = nullptr;

InfoManager *InfoManager::instance()
{
    if (!m_instance) m_instance = new InfoManager;
    return m_instance;
}

InfoManager::InfoManager(QObject *parent) : QObject(parent) {}
