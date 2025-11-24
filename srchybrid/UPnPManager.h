#pragma once
#include <string>
#include <vector>
#include ".\miniupnpc\miniupnpc.h"
#include ".\miniupnpc\upnpcommands.h"
#include ".\miniupnpc\upnperrors.h"
class UPnPManager {
public:
    UPnPManager();
    ~UPnPManager();

    // 初始化UPnP发现
    bool Initialize(int delay = 2000, const char* multicastIf = "0.0.0.0");

    // 添加端口映射
    bool AddPortMapping(int localPort, int publicPort, const std::string& protocol = "TCP",
        const std::string& description = "Port Mapping");

    // 删除端口映射
    bool DeletePortMapping(int publicPort, const std::string& protocol = "TCP");

    // 获取局域网IP
    std::string GetLocalIP() const;
    bool isInitialized();
    // 清理资源
    void Cleanup();

private:
    struct UPNPDev* m_devlist;
    struct UPNPUrls m_urls;
    struct IGDdatas m_data;
    char m_lanaddr[64];
    bool m_initialized;
};