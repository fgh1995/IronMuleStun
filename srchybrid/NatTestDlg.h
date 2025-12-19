#pragma once

#include <windows.h>
#include <vector>

class CNatTestDlg
{
struct StunServerInfo {
    std::string hostname;
    unsigned short port;
    std::string description;
};
public:
    CNatTestDlg();
    ~CNatTestDlg();

    // 显示模态对话框（需要WS_POPUP样式）
    INT_PTR ShowModal(HINSTANCE hInstance, HWND hParent = NULL);

    // 创建非模态对话框（支持WS_CHILD或WS_POPUP）
    BOOL Create(HINSTANCE hInstance, HWND hParent = NULL);

    // 作为子窗口创建（必须使用WS_CHILD样式）
    BOOL CreateAsChild(HINSTANCE hInstance, HWND hParent, int x = 0, int y = 0);

    // 关闭对话框
    void Close();

    // 销毁对话框
    void Destroy();

    // 获取窗口句柄
    HWND GetHWnd() const { return m_hWnd; }

    // 获取编辑框句柄
    HWND GetEditHWnd() const { return m_hEdit; }

    // 检查窗口是否可见
    BOOL IsVisible() const;

    // 启用调试信息
    void EnableDebug(BOOL bEnable = TRUE) { m_bDebug = bEnable; }

    // 设置窗口文本
    BOOL SetWindowText(LPCTSTR lpszText);

    // 添加日志文本
    void AddLogText(LPCTSTR lpszText);

private:
    // 对话框过程
    static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 子窗口对话框过程
    static INT_PTR CALLBACK ChildDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 消息处理
    INT_PTR HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 初始化对话框
    BOOL OnInitDialog(HWND hWnd);

    // 处理命令消息
    void OnCommand(WPARAM wParam);

    void ParseStunServers(const CString& strConfig, std::vector<StunServerInfo>& servers);

    // 处理关闭消息
    void OnClose(HWND hWnd);

    // 输出调试信息
    void DebugMessage(LPCTSTR pszFormat, ...);

    // 检查资源
    BOOL CheckDialogResource(HINSTANCE hInstance);

private:
    HWND        m_hWnd;         // 对话框句柄
    HWND        m_hEdit;        // 编辑框句柄
    HINSTANCE   m_hInstance;    // 实例句柄
    BOOL        m_bDebug;       // 调试标志
    BOOL        m_bModal;       // 是否为模态对话框
    BOOL        m_bChild;       // 是否为子窗口
};

// 全局指针，用于在对话框过程中访问类实例
extern CNatTestDlg* g_pCurrentNatTestDlg;