#include "global.h"
#include "Firewall.h"
#include "Win32Exception.h"
#include "resource.h"
#include <Windows.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <utility>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/debug_output_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#pragma comment(lib, "LibFirewall.lib")

using namespace Win32Util;
using namespace ::WfpUtil;
namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

static std::shared_ptr<CFirewall> pFirewall = nullptr;

INT_PTR CALLBACK DialogFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), HWND_DESKTOP, (DLGPROC)DialogFunc);
    return 0;
}


INT_PTR CALLBACK DialogFunc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    static std::map<UINT, HWND> hWndCtrl;
    static std::map<UINT, HWND> hWndEdit;
    static HWND hWndComboBox = nullptr;
    static HWND hWndList = nullptr;
    static HWND hWndTextAllBlock = nullptr;

    static bool isAllBlock = false;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        //UI�p�[�c�̐ݒ�

        {
            BuildStringMap();

            //���x���t���R���g���[��
            for (const auto& elem : ctrlToLabel)
            {
                HWND hWndItem = GetDlgItem(hWndDlg, elem.first);
                SetWindowText(hWndItem, stringMap.at(elem.second).c_str());
                hWndCtrl[elem.first] = hWndItem;
            }

            //edit�e�L�X�g
            for (const auto& elem : chkIDToEditID)
            {
                HWND hWndItem = GetDlgItem(hWndDlg, elem.second);
                hWndEdit[elem.second] = hWndItem;
            }

            //�t�B���^�[�A�N�V�����p�̃R���{�{�b�N�X
            hWndComboBox = GetDlgItem(hWndDlg, IDC_COMBO);
            SendMessage(hWndComboBox, CB_ADDSTRING, 0, (LPARAM)stringMap.at(IDS_COMBO_SEL_PERMIT).c_str());
            SendMessage(hWndComboBox, CB_ADDSTRING, 0, (LPARAM)stringMap.at(IDS_COMBO_SEL_BLOCK).c_str());
            constexpr DWORD INIT_COMBO_SEL = 0;
            SendMessage(hWndComboBox, CB_SETCURSEL, INIT_COMBO_SEL, 0);

            //�t�B���^�[�\���p���X�g
            hWndList = GetDlgItem(hWndDlg, IDC_LIST);

            //�S�Ւf�p�̃e�L�X�g
            hWndTextAllBlock = GetDlgItem(hWndDlg, IDC_TEXT_ALLBLOCK);
            SetWindowText(hWndTextAllBlock, _T(""));


            //������ԂŃ`�F�b�N���
            SendMessage(hWndCtrl[IDC_CHECK_PROCESS], BM_SETCHECK, BST_CHECKED, 0);

            //������ԂŖ�����
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)IDC_CHECK_ADDR    , (LPARAM)IDC_IPADDRESS);
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)IDC_CHECK_PORT    , (LPARAM)IDC_EDIT_PORT);
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)IDC_CHECK_FQDN    , (LPARAM)IDC_EDIT_FQDN);
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)IDC_CHECK_PROTOCOL, (LPARAM)IDC_EDIT_PROTOCOL);
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)IDC_CHECK_URL     , (LPARAM)IDC_EDIT_URL);

        }

        boost::program_options::options_description opt;
        opt.add_options()
            ("trace,t", "");
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(__argc, __wargv, opt), vm);
        boost::program_options::notify(vm);
        if (vm.count("trace"))
        {
            logging::add_common_attributes();
            logging::add_file_log(
                keywords::file_name = "trace.log", // log���o�͂���t�@�C����
                keywords::format    = "%Tag%: [%TimeStamp%] [%ThreadID%] %Message%" // log�̃t�H�[�}�b�g
            );
        }

        try
        {
            pFirewall = std::make_shared<CFirewall>();
        }
        catch (std::runtime_error& e)
        {
            BOOST_LOG_TRIVIAL(trace) << "CFirewall::CFirewall failed with error: " << e.what();
            MessageBox(hWndDlg, stringMap[IDS_ERROR_FW_INIT].c_str(), _T(""), MB_ICONERROR | MB_OK);
            exit(1);
            break;
        }

        return (INT_PTR)TRUE;
    }
    case WM_CLOSE:
        try
        {
            pFirewall->close();
        }
        catch (std::runtime_error& e)
        {
            BOOST_LOG_TRIVIAL(trace) << "CFirewall::close failed with error: " << e.what();
            MessageBox(hWndDlg, stringMap.at(IDS_ERROR_FW_EXIT).c_str(), _T(""), MB_ICONERROR | MB_OK);
            exit(1);
            break;
        }
        EndDialog(hWndDlg, 0);
        return (INT_PTR)TRUE;
    case FWM_IP_CHECK:
    {
        UINT chkID = (UINT)wParam;
        for (const auto& elem : ipAddrCheckIDAndEditID)
        {
            if (chkID != elem.first)
            {
                SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)elem.first, (LPARAM)elem.second);
            }
        }
        return (INT_PTR)TRUE;
    }
    case FWM_PORT_CHECK:
    {
        UINT chkID = (UINT)wParam;
        for (const auto& elem : portCheckIDAndEditID)
        {
            if (wParam != elem.first)
            {
                SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)elem.first, (LPARAM)elem.second);
            }
        }
        return (INT_PTR)TRUE;
    }
    case FWM_DISABLE_FORM:
    {
        UINT chkID  = (UINT)wParam;
        UINT editID = (UINT)lParam;
        SendMessage(hWndCtrl[chkID] , BM_SETCHECK   , BST_UNCHECKED, 0);
        SendMessage(hWndEdit[editID], EM_SETREADONLY, TRUE         , 0);
        return (INT_PTR)TRUE;
    }
    case FWM_CHECKBOX:
    {
        UINT chkID = (UINT)wParam;
        UINT editID = (UINT)lParam;
        bool isChecked = BST_CHECKED == SendMessage(hWndCtrl[chkID], BM_GETCHECK, 0, 0);

        //�`�F�b�N���O�����ꍇ�̓t�H�[���𖳌�������
        if (!isChecked)
        {
            SendMessage(hWndDlg, FWM_DISABLE_FORM, wParam, lParam);
            return (INT_PTR)TRUE;
        }

        SendMessage(hWndEdit[editID], EM_SETREADONLY, FALSE, 0);

        //IP�A�h���X�AFQDN�AURL�Ƀ`�F�b�N�����ꍇ�͎��g�ȊO�̃`�F�b�N���O��
        //��FFQDN�Ƀ`�F�b�N ---> IP�A�h���X��URL�̃`�F�b�N���O��
        bool isIpCheck = ipAddrCheckIDAndEditID.end() != std::find(
            ipAddrCheckIDAndEditID.begin(), 
            ipAddrCheckIDAndEditID.end(), 
            std::make_pair(chkID, editID)
        );

        if (isIpCheck)
        {
            SendMessage(hWndDlg, FWM_IP_CHECK, wParam, 0);
        }

        //�|�[�g�ԍ��A�v���g�R���AURL�Ƀ`�F�b�N�����ꍇ�͎��g�ȊO�̃`�F�b�N���O��
        bool isPortCheck = portCheckIDAndEditID.end() != std::find(
            portCheckIDAndEditID.begin(), 
            portCheckIDAndEditID.end(), 
            std::make_pair(chkID,editID)
        );
        if (isPortCheck)
        {
            SendMessage(hWndDlg, FWM_PORT_CHECK, wParam, 0);
        }
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_ADD:
        {
            //��d������h�����ߖ��������Ă���
            EnableWindow(hWndCtrl[IDC_BUTTON_ADD], FALSE);

            std::wstringstream ssListItem;
            bool isPermit;
            try
            {
                for (const auto& elem : chkIDToEditID)
                {
                    if (SendMessage(hWndCtrl[elem.first], BM_GETCHECK, 0, 0) != BST_CHECKED)
                    {
                        continue;
                    }

                    constexpr int BUFFER_LENGTH = 1024;
                    std::vector<CHAR> sEditBuffer(BUFFER_LENGTH);
                    GetWindowTextA(hWndEdit[elem.second], sEditBuffer.data(), BUFFER_LENGTH);
                    
                    if (elem.first == IDC_CHECK_ADDR)
                    {
                        pFirewall->AddIpAddrCondition(sEditBuffer.data());
                    }
                    else if (elem.first == IDC_CHECK_PORT)
                    {
                        pFirewall->AddPortCondition(std::atoi(sEditBuffer.data()));
                    }
                    else if (elem.first == IDC_CHECK_FQDN)
                    {
                        pFirewall->AddFqdnCondition(sEditBuffer.data());
                    }
                    else if (elem.first == IDC_CHECK_PROTOCOL)
                    {
                        pFirewall->AddPortCondition(sEditBuffer.data());
                    }
                    else if (elem.first == IDC_CHECK_URL)
                    {
                        pFirewall->AddUrlCondition(sEditBuffer.data());
                    }
                    else if (elem.first == IDC_CHECK_PROCESS)
                    {
                        pFirewall->AddProcessCondition(sEditBuffer.data());
                    }
                    ssListItem << sEditBuffer.data() << ", ";
                }
                isPermit = 0 == (int)SendMessage(hWndComboBox, CB_GETCURSEL, 0, 0);
                pFirewall->AddFilter(isPermit ? FW_ACTION_PERMIT : FW_ACTION_BLOCK);
            }
            catch (std::runtime_error& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::AddFilter failed with error: " << e.what();
                MessageBox(hWndDlg, stringMap.at(IDS_ERROR_FW_ADD_FILTER).c_str(), _T(""), MB_ICONERROR | MB_OK);
                EnableWindow(hWndCtrl[IDC_BUTTON_ADD], TRUE);
                break;
            }

            ssListItem << (isPermit ? stringMap.at(IDS_COMBO_SEL_PERMIT) : stringMap.at(IDS_COMBO_SEL_BLOCK));

            //ListBox�̖����ɒǉ�(��3������-1���w��)
            SendMessage(hWndList, LB_INSERTSTRING, -1, (LPARAM)ssListItem.str().c_str());

            for (const auto& elem : hWndEdit)
            {
                SetWindowText(elem.second, _T(""));
            }
            EnableWindow(hWndCtrl[IDC_BUTTON_ADD], TRUE);
            return (INT_PTR)TRUE;
        }
        case IDC_BUTTON_DEL:
        {
            //��d������h�����ߖ��������Ă���
            EnableWindow(hWndCtrl[IDC_BUTTON_DEL], FALSE);

            //���I���̏ꍇ��-1���Ԃ��Ă���
            LRESULT idx = SendMessage(hWndList, LB_GETCURSEL, 0, 0);
            if (idx == -1)
            {
                EnableWindow(hWndCtrl[IDC_BUTTON_DEL], TRUE);
                return (INT_PTR)TRUE;
            }
            int id = MessageBox(hWndDlg, stringMap.at(IDS_CONFIRM_FW_RM_FILTER).c_str(), _T(""), MB_OKCANCEL | MB_ICONEXCLAMATION);

            if (id == IDCANCEL)
            {
                EnableWindow(hWndCtrl[IDC_BUTTON_DEL], TRUE);
                return (INT_PTR)TRUE;
            }

            try
            {
                pFirewall->RemoveFilter(idx);
            }
            catch (std::runtime_error& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::RemovingFilter failed with error: " << e.what();
                MessageBox(hWndDlg, stringMap.at(IDS_ERROR_FW_RM_FILTER).c_str(), _T(""), MB_ICONERROR | MB_OK);
                EnableWindow(hWndCtrl[IDC_BUTTON_DEL], TRUE);
                break;
            }
            SendMessage(hWndList, LB_DELETESTRING, idx, 0);
            EnableWindow(hWndCtrl[IDC_BUTTON_DEL], TRUE);
            return (INT_PTR)TRUE;        
        }
        case IDC_BUTTON_ALLBLOCK:
        {
            //��d������h�����ߖ��������Ă���
            EnableWindow(hWndCtrl[IDC_BUTTON_ALLBLOCK], FALSE);
            try
            {
                pFirewall->AllBlock(!isAllBlock, FW_DIRECTION_OUTBOUND);
            }
            catch (const std::runtime_error& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::AllBlock failed with error: " << e.what();
                MessageBox(hWndDlg, stringMap.at(IDS_ERROR_FW_ADD_FILTER).c_str(), _T(""), MB_ICONERROR | MB_OK);
                EnableWindow(hWndCtrl[IDC_BUTTON_ALLBLOCK], TRUE);
                break;
            }
            isAllBlock = !isAllBlock;
            SetWindowText(hWndCtrl[IDC_BUTTON_ALLBLOCK], isAllBlock ? stringMap.at(IDS_BTN_LABEL_ALLBLOCK_DISABLE).c_str() : stringMap.at(IDS_BTN_LABEL_ALLBLOCK_ENABLE).c_str());
            SetWindowText(hWndTextAllBlock, isAllBlock ? stringMap.at(IDS_STATIC_TEXT_ALLBLOCK_ENABLE).c_str() : _T(""));
            EnableWindow(hWndCtrl[IDC_BUTTON_ALLBLOCK], TRUE);
            return (INT_PTR)TRUE;

        }
        case IDC_CHECK_ADDR:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)IDC_CHECK_ADDR, (LPARAM)IDC_IPADDRESS);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_FQDN:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)IDC_CHECK_FQDN, (LPARAM)IDC_EDIT_FQDN);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_PORT:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)IDC_CHECK_PORT, (LPARAM)IDC_EDIT_PORT);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_PROTOCOL:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)IDC_CHECK_PROTOCOL, (LPARAM)IDC_EDIT_PROTOCOL);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_URL:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)IDC_CHECK_URL, (LPARAM)IDC_EDIT_URL);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_PROCESS:
        {
            bool isChecked = BST_CHECKED == SendMessage(hWndCtrl[IDC_CHECK_PROCESS], BM_GETCHECK, 0, 0);

            //�`�F�b�N���O�����ꍇ�t�H�[���𖳌�������
            SendMessage(hWndEdit[IDC_EDIT_PROCESS], EM_SETREADONLY, !isChecked, 0);
            
            return (INT_PTR)TRUE;
        }
        }   //switch (LOWORD(wParam))
        break;
    }
    return (INT_PTR)FALSE;
}