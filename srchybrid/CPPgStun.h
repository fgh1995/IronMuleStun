// CPPgStun.h
#pragma once

// 包含必要的头文件
#include "Resource.h"

// 前置声明
class CStunSettingsPage : public CPropertyPage
{
    DECLARE_DYNAMIC(CStunSettingsPage)

public:
    CStunSettingsPage();
    virtual ~CStunSettingsPage();

    // 对话框数据
    enum { IDD = IDD_PPG_STUN };

    // 公共方法
    void Localize();           // 本地化
    void LoadSettings();       // 加载设置
    void SaveSettings();       // 保存设置

protected:
    // 控件成员变量
    CEdit m_ctlStunServer;     // STUN 服务器编辑框
    CButton m_ctlEnableStun;   // 启用 STUN 复选框

    // 其他成员变量
    CString m_strStunServer;   // STUN 服务器地址
    BOOL m_bEnableStun;        // 是否启用 STUN
    CString m_strOriginalStunServer; // 原始服务器地址（用于比较是否修改）

    // 保护方法
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();
    void OnStunEnabledChanged(BOOL isEnableStuun);
    virtual void OnOK();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

    // 验证输入
    BOOL ValidateStunServer(CString strServer);
    void UpdateControls();      // 根据启用状态更新控件
    BOOL ValidateSingleStunServer(CString strServer);
    DECLARE_MESSAGE_MAP()

    // 消息处理函数
    afx_msg void OnBnClickedEnableStun();
    afx_msg void OnEnChangeStunServer();
    afx_msg void OnBnClickedTestStun();
    afx_msg void OnHelp();
    afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
public:
    afx_msg void OnBnClickedGetStunServer();
};