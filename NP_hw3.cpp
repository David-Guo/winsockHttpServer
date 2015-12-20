#include <windows.h>
#include <list>
#include <assert.h>
#include <vector>
#include "client.h"
using namespace std;

#include "resource.h"

#define SERVER_PORT 7799

#define WM_SOCKET_NOTIFY (WM_USER + 1)
#define WM_SOCKET_NOTIFY_SERVER (WM_USER + 2)

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
//=================================================================
//	Global Variables
//=================================================================
list<SOCKET> Socks;
clientHandler globalClientHandler;

void decodeEnv(string sourceString, vector<string>& vDest) {
	assert(sourceString != "");
	stringstream source(sourceString);
	string splited;
	while(getline(source, splited, '&'))
		vDest.push_back(splited);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
				   LPSTR lpCmdLine, int nCmdShow)
{

	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	static HWND hwndEdit;
	static SOCKET msock, ssock;
	static struct sockaddr_in sa;

	int err;


	switch(Message) 
	{
	case WM_INITDIALOG:
		hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case ID_LISTEN:

			WSAStartup(MAKEWORD(2, 0), &wsaData);

			//create master socket
			msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			if( msock == INVALID_SOCKET ) {
				EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
				WSACleanup();
				return TRUE;
			}

			err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

			if ( err == SOCKET_ERROR ) {
				EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
				closesocket(msock);
				WSACleanup();
				return TRUE;
			}

			//fill the address info about server
			sa.sin_family		= AF_INET;
			sa.sin_port			= htons(SERVER_PORT);
			sa.sin_addr.s_addr	= INADDR_ANY;

			//bind socket
			err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

			if( err == SOCKET_ERROR ) {
				EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
				WSACleanup();
				return FALSE;
			}

			err = listen(msock, 2);

			if( err == SOCKET_ERROR ) {
				EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
				WSACleanup();
				return FALSE;
			}
			else {
				EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
			}

			break;
		case ID_EXIT:
			EndDialog(hwnd, 0);
			break;
		};
		break;

	case WM_CLOSE:
		EndDialog(hwnd, 0);
		break;

	case WM_SOCKET_NOTIFY:
		switch( WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT:
			ssock = accept(msock, NULL, NULL);
			Socks.push_back(ssock);
			EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
			break;
		case FD_READ:
			//Write your code for read event here.
			for (std::list<SOCKET>::iterator it = Socks.begin(); it != Socks.end(); it++) {
				char *requestBuff;
				requestBuff = new char[1024];
				int recvStates = recv(*it, requestBuff, 1024*sizeof(char), 0);
				// 接受消息失败
				if (recvStates <= 0) {
					delete[] requestBuff;
					EditPrintf(hwndEdit, TEXT("recv error"));
					continue;
				}
				// 接受成功
				EditPrintf(hwndEdit, TEXT("request: "));
				EditPrintf(hwndEdit, TEXT(requestBuff));
				EditPrintf(hwndEdit, TEXT("\r\n"));

				// 解析协议 得到querystring 环境变量与执行程序名
				string recvMsg;
				recvMsg = string(requestBuff);
				stringstream ss(recvMsg);
				string queryString;
				string requestDoc;
				string requestLine;
				string offset;

				// 取出请求行
				getline(ss, requestLine, '\n');
				stringstream ssRqstLine(requestLine);

				if (requestLine.find(".cgi") == string::npos) 
				{
					delete[] requestBuff;
					EditPrintf(hwndEdit, TEXT("not request a cgi doc"));
					continue;
				}
				// 请求 cgi 文档
				globalClientHandler.addNewClient(*it);
				EditPrintf(hwndEdit, TEXT("adding new client: %d\r\n"),*it);

				getline(ssRqstLine, offset, '/');
				getline(ssRqstLine, offset, '?');
				requestDoc = offset;
				getline(ssRqstLine, offset, ' ');
				queryString = offset;
				// 开启终端查看
				//AllocConsole();
				//freopen("CONOUT$", "w", stderr);
				//cerr << "requestDoc: " << requestDoc << endl;
				//cerr << "queryString: " << queryString << endl;

				// 生成响应头
				singleClient *singleClie;
				singleClie = globalClientHandler.findClient(*it);
				string response = string("HTTP/1.1 200 OK\r\n");
				response += string("response head\r\n");
				response += string("\r\n");
				send(singleClie->serverSocketFD, response.c_str(), response.size(), 0);

				// 生成 html 表格标签
				string htmltable = string("<html>")
					+ string("<head>")
					+ string("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />")
					+ string("<title>Network Programming Homework 3</title>")
					+ string("</head>")
					+ string("<body bgcolor=#336699>")
					+ string("<font face=\"Courier New\" size=2 color=#FFFF99>")
					+ string("<table width=\"800\" border=\"1\">")
					+ string("<tr>");
				send(singleClie->serverSocketFD, htmltable.c_str(), htmltable.size(), 0);

				// CGI 部分，解析环境变量 queryString
				vector<string> vDecodeEnv;
				decodeEnv(queryString, vDecodeEnv);

				bool isInvalid = false;
				for (int i = 0; i < vDecodeEnv.size(); i += 3) {
					// 符号 = 后面为空，即为无效的请求
					if (vDecodeEnv[0].find_first_of('=') == vDecodeEnv[0].size() - 1) {
						isInvalid = true;
						break;
					}
					else {
						string serverIP = vDecodeEnv[i].substr(vDecodeEnv[i].find_first_of('=') + 1);
						string serverPort = vDecodeEnv[i + 1].substr(vDecodeEnv[i + 1].find_first_of('=') + 1);
						string serverFile = vDecodeEnv[i + 2].substr(vDecodeEnv[i + 2].find_first_of('=') + 1);

						// 初始化 winsock
						int ithResult;
						WSADATA wsaData;
						ithResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
						if (ithResult != NO_ERROR) {
							EditPrintf(hwndEdit, TEXT("=== win socket start up fail ===\r\n"));
							return 1;
						}

						// 创建 socket
						SOCKET m_socket;
						m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
						if (m_socket == INVALID_SOCKET) {
							EditPrintf(hwndEdit, TEXT("=== creat socket fail ===\r\n"));
							WSACleanup();
							return FALSE;
						}

						// 设定 ras server 地址端口信息
						struct hostent *he;
						he = gethostbyname(serverIP.c_str());
						if (he == NULL) {
							EditPrintf(hwndEdit, TEXT("=== error: getting host name ===\r\n"));
							WSACleanup();
							return FALSE;
						}
						SOCKADDR_IN client_sin;
						memset(&client_sin, 0, sizeof(client_sin));
						client_sin.sin_family = AF_INET;
						client_sin.sin_addr.s_addr = *((unsigned long long *)he->h_addr);
						client_sin.sin_port = htons(atoi(serverPort.c_str()));
						EditPrintf(hwndEdit, TEXT("=== finish setting server ===\n"));

						// 注册同步事件（event）
						err = WSAAsyncSelect(m_socket, hwnd, WM_SOCKET_NOTIFY_SERVER, FD_CLOSE | FD_READ | FD_WRITE);
						if ( err == SOCKET_ERROR ) {
							EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
							closesocket(m_socket);
							WSACleanup();
							return TRUE;
						}
						// 连接到 ras 服务器
						connect(m_socket, (SOCKADDR *)&client_sin, sizeof(client_sin));
						EditPrintf(hwndEdit, TEXT("=== connection build with server ===\r\n"));

						// 初始化 client job
						singleClient *singleClie;
						singleClie = globalClientHandler.findClient(*it);
						if (singleClie == NULL); // 没有这个 client
						else {
							singleClie->init(*it);
							Client *rasClient;
							rasClient = singleClie->getANewJob();
							// UserNum 减去 1 之后才能与 HTML 表格标签的 id 属性挂钩
							rasClient->init(serverIP, serverPort, serverFile, singleClie->currentUserNum - 1, m_socket);
							EditPrintf(hwndEdit, TEXT("=== successful init a client ===\r\n"));
						}
					}

				} // 结束对 vDecodeEnv.size() 的迭代，表示一个请求初始化结束

				if (isInvalid == true) { 
					// 无效的请求
					delete[] requestBuff;
					continue;
				}
				string htmlpage;
				// html 表格标签
				for(int i = 0; i < singleClie->getUserNum(); i++) {
					Client *rasClient;
					rasClient = singleClie->getAnExisJob(i);
					if(rasClient->IP == "") continue;
					else 
						htmlpage += string("<td>") + rasClient->IP + string("</td>");
				}
				htmlpage += string("</tr></tr>");
				// 标签 id 属性，使用 javascript 填充内容
				for(int i = 0; i < singleClie->getUserNum(); i++) {
					Client *rasClient;
					rasClient = singleClie->getAnExisJob(i);
					if(rasClient->Port == "") continue;
					else 
						htmlpage += string("<td valign=\"top\" id=\"m") + string(to_string((unsigned long long)i)) + string("\"></td>");
				}
				string webpage2;
				webpage2 += string("</tr></table>\n");
				// 向浏览器发送 html css 标签
				send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(),0);
				send(singleClie->serverSocketFD, webpage2.c_str(), webpage2.size(),0);

				delete[] requestBuff;
			} 	// 结束对 Socks.size() 的迭代

			break;
		case FD_WRITE:
			//Write your code for write event here

			break;
		case FD_CLOSE:
			break;
		};
		break;  // 结束 WM_SOCKET_NOTIFY: 事件

	case WM_SOCKET_NOTIFY_SERVER:
		switch( WSAGETSELECTEVENT(lParam)) {
		case FD_READ:
			// vClient.size() 表示浏览器请求数量
			for (int k = 0; k < globalClientHandler.vClients.size(); k++) {
				singleClient *singleClie;
				singleClie = &(globalClientHandler.vClients[k]);

				// userNum 表示 ras 数量
				for (int j = 0; j < singleClie->getUserNum(); j++) {
					Client *rasClient;
					rasClient = singleClie->getAnExisJob(j);
					// 从 ras 接受数据
					rasClient->reciveFromServ();
					string htmlpage = rasClient->returnRsltHtml() + string("\n");
					// 向浏览器发送 从 ras 接受到的数据
					send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(), 0);

					if (rasClient->isSend && rasClient->isExit) 
						continue;
					// 可以向 ras 发送从文件读取的指令
					if (rasClient->isSend)
						rasClient->sendToServ();
					Sleep(1000);
					// ????
					if(rasClient->isSend && rasClient->isExit) continue;

					if (rasClient->isExit) {
						Sleep(3000);
						// 从 ras 读取最后一次离开信息，连同 exit 指令一起发送给浏览器
						rasClient->reciveFromServ();
						htmlpage = rasClient->returnRsltHtml() + string("\n");
						send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(), 0);
						//continue;
					}
					else {
						// 向浏览器发送文件指令
						htmlpage = rasClient->returnRsltHtml() + string("\n");
						send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(), 0);
						rasClient->isSend = FALSE;
					}
				}

				// 检查是否结束了输出 HTML 
				bool isAllExit = true;
				for (int i = 0; i < singleClie->getUserNum(); i++) {
					Client *rasClient;
					rasClient = singleClie->getAnExisJob(i);

					if(((rasClient->isExit == TRUE) && (rasClient->isSend)) == FALSE) {
						isAllExit = FALSE;
						break;
					}
				}
				if (isAllExit) {
					EditPrintf(hwndEdit, TEXT("===  IS READY TO OUTPUT A WEBPAGE ==="));
					string endHTML = "";
					endHTML += string("</font>")
						+ string("</body>")
						+ string("</html>\n");

					send(singleClie->serverSocketFD, endHTML.c_str(), endHTML.size(), 0);
					closesocket(singleClie->serverSocketFD);
					globalClientHandler.deleteClient(singleClie->serverSocketFD);
					k--;
				}

			}
		case FD_WRITE:
			break;
		case FD_CLOSE:
			break;
		}
		break; // 结束 WM_SOCKET_NOTIFY_SERVER: 事件

	default:
		return FALSE;

	};

	return TRUE;
}

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
	TCHAR   szBuffer [2048] ;
	va_list pArgList ;

	va_start (pArgList, szFormat) ;
	wvsprintf (szBuffer, szFormat, pArgList) ;
	va_end (pArgList) ;

	SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
	SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
	SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
	return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0); 
}