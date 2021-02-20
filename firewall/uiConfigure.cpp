#include "uiConfigure.h"
#include "resource.h"
#include <utility>
#include <vector>

void InitCheckBox(HWND hWndParent, std::map<std::string, HWND>& hWndCheckBox)
{
    //key -> (�\���e�L�X�g,���\�[�XID)
    static const std::map<std::string, std::pair<std::wstring, int>> chkBoxResources = {
        {"addr"    ,{L"IP�A�h���X"                 ,IDC_CHECK_ADDR}},
        {"port"    ,{L"�|�[�g�ԍ�"                 ,IDC_CHECK_PORT}},
        {"fqdn"    ,{L"FQDN"                       ,IDC_CHECK_FQDN}},
        {"protocol",{L"�v���g�R��"                 ,IDC_CHECK_PROTOCOL}} ,
        {"url"     ,{L"URL"                        ,IDC_CHECK_URL}},
        {"process" ,{L"�v���Z�X(exe�܂ł̃t���p�X)",IDC_CHECK_PROCESS}},
    };

    for (const auto& elem : chkBoxResources)
    {
        HWND hWndItem = GetDlgItem(hWndParent, elem.second.second);
        SetWindowText(hWndItem, elem.second.first.c_str());
        hWndCheckBox[elem.first] = hWndItem;
    }
}

void InitButton(HWND hWndParent, std::map<std::string, HWND>& hWndButton)
{
    //key -> (�\���e�L�X�g,���\�[�XID)
    static const std::map<std::string, std::pair<std::wstring, int>> btnResources = {
        {"add"     , {L"�ǁ@��" ,IDC_BUTTON_ADD} },
        {"del"     , {L"��@��" ,IDC_BUTTON_DEL} },
        {"allBlock", {L"�S�Ւf" ,IDC_BUTTON_ALLBLOCK}},
    };

    for (const auto& elem : btnResources)
    {
        HWND hWndItem = GetDlgItem(hWndParent, elem.second.second);
        SetWindowText(hWndItem, elem.second.first.c_str());
        hWndButton[elem.first] = hWndItem;
    }
}

void InitEdit(HWND hWndParent, std::map<std::string, HWND>& hWndEdit)
{
    //(key,���\�[�XID)
    static const std::vector<std::pair<std::string, int>> editResources = {
        {"addr"    ,IDC_IPADDRESS},
        {"port"    ,IDC_EDIT_PORT},
        {"fqdn"    ,IDC_EDIT_FQDN},
        {"protocol",IDC_EDIT_PROTOCOL},
        {"url"     ,IDC_EDIT_URL},
        {"process" ,IDC_EDIT_PROCESS},
    };

    for (const auto& elem : editResources)
    {
        HWND hWndItem = GetDlgItem(hWndParent, elem.second);
        hWndEdit[elem.first] = hWndItem;
    }
}

void InitComboBox(HWND hWndParent, HWND& hWndCombo) 
{

    SetDlgItemText(hWndParent, IDC_TEXT_ACTION, /*label = */L"�A�N�V����");

    static const std::vector<std::wstring> comboText = {
        L"����",
        L"�Ւf",
    };

    static constexpr DWORD INIT_COMBO_SEL = 0;

    hWndCombo = GetDlgItem(hWndParent, IDC_COMBO);

    for (const auto& elem : comboText)
    {
        SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)elem.c_str());
    }

    SendMessage(hWndCombo, CB_SETCURSEL, INIT_COMBO_SEL, 0);
}