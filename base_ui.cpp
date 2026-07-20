#include <windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

#define created using
#define by namespace
#define DE_RatIO std
created by DE_RatIO;

unsigned char enctab[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&()*+,./:;-=\\?@[]^_`{|}~\'",dectab[256]={};

namespace base91{
	bool encode(string&out,const string&binary){
		out.clear();
		const unsigned char*ib=(unsigned char*)binary.c_str();
		unsigned long queue=0;
		unsigned int nbits=0;
		for(size_t len=binary.size();len--;){
			queue|=*ib++<<nbits;nbits+=8;
			if(nbits>13){
				unsigned int val=queue&8191;
				if(val>88){queue>>=13;nbits-=13;}
				else{val=queue&16383;queue>>=14;nbits-=14;}
				out.push_back(enctab[val%91]);
				out.push_back(enctab[val/91]);
			}
		}
		if(nbits){
			out.push_back(enctab[queue%91]);
			if(nbits>7||queue>90)
				out.push_back(enctab[queue/91]);
		}return true;
	}
	bool decode(string&out,const string&text){
		out.clear();
		const unsigned char*ib=(unsigned char*)text.c_str();
		unsigned long queue=0;unsigned int nbits=0;int val=-1;
		for(size_t len=text.size();len--;){
			unsigned int d=dectab[*ib++];
			if(d==91)continue;if(val==-1)val=d;
			else{
				val+=d*91;queue|=val<<nbits;
				nbits+=(val&8191)>88?13:14;
				do{	out.push_back(char(queue));
					queue>>=8;nbits-=8;
				}while(nbits>7);val=-1;
			}
		}
		if(val!=-1)out.push_back(char(queue|val<<nbits));
		return true;
	}
	string encode(const string&binary){
		string out;return base91::encode(out,binary)?out:string();
	}string decode(const string&text){
		string out;return base91::decode(out,text)?out:string();
	}
}

const wchar_t CLASS_NAME[]  = L"Base91EncodeDecodeWindow";
const wchar_t WINDOW_TITLE[] = L"Base91 编码/解码器";

HWND hEditInput, hEditStatus, hBtnDecode, hBtnEncode;
HFONT hFont;

void SetStatusText(const wchar_t* text) {
    SetWindowTextW(hEditStatus, text);
}

string WideToUTF8(const wchar_t* wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    string str(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], len, NULL, NULL);
    return str;
}

string WideToMulti(const wchar_t* wstr) {
    int len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    string str(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, &str[0], len, NULL, NULL);
    return str;
}

void CopyToClipboard(const string& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            LPVOID lpMem = GlobalLock(hMem);
            memcpy(lpMem, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

void HandleEncode() {
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        SetStatusText(L"正在读取文件...");

        string fin = WideToMulti(ofn.lpstrFile);
        ifstream in(fin.c_str(), ios::in | ios::binary);
        if (!in) {
            SetStatusText(L"错误：无法打开文件");
            return;
        }

        ostringstream oss;
        oss << in.rdbuf();
        string content = oss.str();

        SetStatusText(L"正在编码...");
        string encoded = base91::encode(content);

        CopyToClipboard(encoded);

        wchar_t status[512];
        swprintf_s(status, L"编码完成！已复制到剪贴板。原文件: %zu 字节，编码后: %zu 字节", content.size(), encoded.size());
        SetStatusText(status);
    }
}

void HandleDecode() {
    int textLen = GetWindowTextLengthW(hEditInput);
    if (textLen == 0) {
        SetStatusText(L"错误：输入框为空");
        return;
    }

    wstring input(textLen + 1, L'\0');
    GetWindowTextW(hEditInput, &input[0], textLen + 1);

    string utf8Input = WideToUTF8(input.c_str());

    SetStatusText(L"正在解码...");
    string decoded = base91::decode(utf8Input);

    if (decoded.empty()) {
        SetStatusText(L"错误：解码结果为空");
        return;
    }

    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"output.bin";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        string fout = WideToMulti(ofn.lpstrFile);
        ofstream out(fout.c_str(), ios::out | ios::binary | ios::trunc);
        if (!out) {
            SetStatusText(L"错误：无法创建输出文件");
            return;
        }

        out.write(decoded.c_str(), decoded.size());
        out.close();

        wchar_t status[512];
        swprintf_s(status, L"解码完成！已保存到: %ls，大小: %zu 字节", ofn.lpstrFile, decoded.size());
        SetStatusText(status);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

            hEditInput = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"在此粘贴编码文本...",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN,
                10, 10, width - 20, height - 110,
                hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL);
            SendMessageW(hEditInput, EM_SETLIMITTEXT, 0x7FFFFFFF, 0);
            SendMessageW(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnDecode = CreateWindowExW(
                0,
                L"BUTTON",
                L"解码到文件",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, height - 90, (width - 30) / 2, 35,
                hwnd, (HMENU)2001, GetModuleHandle(NULL), NULL);
            SendMessageW(hBtnDecode, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnEncode = CreateWindowExW(
                0,
                L"BUTTON",
                L"编码文件",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                (width - 30) / 2 + 20, height - 90, (width - 30) / 2, 35,
                hwnd, (HMENU)2002, GetModuleHandle(NULL), NULL);
            SendMessageW(hBtnEncode, WM_SETFONT, (WPARAM)hFont, TRUE);

            hEditStatus = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"就绪",
                WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
                10, height - 45, width - 20, 35,
                hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL);
            SendMessageW(hEditStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == 2001) {
                HandleDecode();
            } else if (LOWORD(wParam) == 2002) {
                HandleEncode();
            }
            return 0;
        }

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            SetWindowPos(hEditInput, NULL, 10, 10, width - 20, height - 110, SWP_NOZORDER);
            SetWindowPos(hBtnDecode, NULL, 10, height - 90, (width - 30) / 2, 35, SWP_NOZORDER);
            SetWindowPos(hBtnEncode, NULL, (width - 30) / 2 + 20, height - 90, (width - 30) / 2, 35, SWP_NOZORDER);
            SetWindowPos(hEditStatus, NULL, 10, height - 45, width - 20, 35, SWP_NOZORDER);
            return 0;
        }

        case WM_DESTROY: {
            DeleteObject(hFont);
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    for (auto& c : dectab) { c = 91; }
    for (int i = 0; i < sizeof(enctab); i++) { dectab[enctab[i]] = i; }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"注册窗口类失败！", L"错误", MB_OK | MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 450,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"创建窗口失败！", L"错误", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}