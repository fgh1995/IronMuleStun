#include "stdafx.h"
#include "UPnPManager.h"
#include <cstring>
#include <string>
#include ".\miniupnpc\miniupnpc.h"
#include ".\miniupnpc\upnpcommands.h"
#include ".\miniupnpc\upnperrors.h"
#ifdef _WIN32
#include <stdio.h>
#define snprintf _snprintf
#endif
UPnPManager::UPnPManager()
    : m_devlist(nullptr)
    , m_initialized(false)
{
    memset(m_lanaddr, 0, sizeof(m_lanaddr));
    memset(&m_urls, 0, sizeof(m_urls));
    memset(&m_data, 0, sizeof(m_data));
}

UPnPManager::~UPnPManager() {
    Cleanup();
}

bool UPnPManager::Initialize(int delay, const char* multicastIf) {
    if (m_initialized) {
        return true;
    }

    // 发现UPnP设备
    m_devlist = upnpDiscover(delay, multicastIf, NULL, 0);
    if (!m_devlist) {
        return false;
    }

    // 获取有效的IGD设备
    int result = UPNP_GetValidIGD(m_devlist, &m_urls, &m_data, m_lanaddr, sizeof(m_lanaddr));
    if (result != 1) { // 1表示找到有效的IGD
        freeUPNPDevlist(m_devlist);
        m_devlist = nullptr;
        return false;
    }

    m_initialized = true;
    return true;
}

bool UPnPManager::AddPortMapping(int localPort, int publicPort, const std::string& protocol,
    const std::string& description) {
    if (!m_initialized) {
        return false;
    }

    char localPortStr[16];
    char publicPortStr[16];
    snprintf(localPortStr, sizeof(localPortStr), "%d", localPort);
    snprintf(publicPortStr, sizeof(publicPortStr), "%d", publicPort);

    int result = UPNP_AddPortMapping(m_urls.controlURL, m_data.servicetype,
        publicPortStr, localPortStr, m_lanaddr,
        description.c_str(), protocol.c_str(), NULL);

    return (result == UPNPCOMMAND_SUCCESS);
}

bool UPnPManager::DeletePortMapping(int publicPort, const std::string& protocol) {
    if (!m_initialized) {
        return false;
    }

    char publicPortStr[16];
    snprintf(publicPortStr, sizeof(publicPortStr), "%d", publicPort);

    int result = UPNP_DeletePortMapping(m_urls.controlURL, m_data.servicetype,
        publicPortStr, protocol.c_str(), NULL);

    return (result == UPNPCOMMAND_SUCCESS);
}

std::string UPnPManager::GetLocalIP() const {
    return std::string(m_lanaddr);
}
bool UPnPManager::isInitialized() {
    return m_initialized;
}
void UPnPManager::Cleanup() {
    if (m_initialized) {
        FreeUPNPUrls(&m_urls);
        m_initialized = false;
    }

    if (m_devlist) {
        freeUPNPDevlist(m_devlist);
        m_devlist = nullptr;
    }
}