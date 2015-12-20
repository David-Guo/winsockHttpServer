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
		switch( WSAGETSELECTEVENT(lParam) )
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
				// ������Ϣʧ��
				if (recvStates <= 0) {
					delete[] requestBuff;
					EditPrintf(hwndEdit, TEXT("recv error"));
					continue;
				}
				// ���ܳɹ�
				EditPrintf(hwndEdit, TEXT("request: "));
				EditPrintf(hwndEdit, TEXT(requestBuff));
				EditPrintf(hwndEdit, TEXT("\r\n"));

				// ����Э�� �õ�querystring ����������ִ�г�����
				string recvMsg;
				recvMsg = string(requestBuff);
				stringstream ss(recvMsg);
				string queryString;
				string requestDoc;
				string requestLine;
				string offset;

				// ȡ��������
				getline(ss, requestLine, '\n');
				stringstream ssRqstLine(requestLine);

				if (requestLine.find(".cgi") == string::npos) 
				{
					delete[] requestBuff;
					EditPrintf(hwndEdit, TEXT("not request a cgi doc"));
					continue;
				}
				// ���� cgi �ĵ�
				globalClientHandler.addNewClient(*it);
				EditPrintf(hwndEdit, TEXT("adding new client: %d\r\n"),*it);

				getline(ssRqstLine, offset, '/');
				getline(ssRqstLine, offset, '?');
				requestDoc = offset;
				getline(ssRqstLine, offset, ' ');
				queryString = offset;
				// �����ն˲鿴
				//AllocConsole();
				//freopen("CONOUT$", "w", stderr);
				//cerr << "requestDoc: " << requestDoc << endl;
				//cerr << "queryString: " << queryString << endl;

				// CGI ���֣������������� queryString
				vector<string> vDecodeEnv;
				decodeEnv(queryString, vDecodeEnv);

				bool isInvalid = false;
				for (int i = 0; i < vDecodeEnv.size(); i += 3) {
					// ���� = ����Ϊ�գ���Ϊ��Ч������
					if (vDecodeEnv[0].find_first_of('=') == vDecodeEnv[0].size() - 1) {
						isInvalid = true;
						break;
					}
					else {
						string serverIP = vDecodeEnv[i].substr(vDecodeEnv[i].find_first_of('=') + 1);
						string serverPort = vDecodeEnv[i + 1].substr(vDecodeEnv[i + 1].find_first_of('=') + 1);
						string serverFile = vDecodeEnv[i + 2].substr(vDecodeEnv[i + 2].find_first_of('=') + 1);

						// ��ʼ�� winsock
						int ithResult;
						WSADATA wsaData;
						ithResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
						if (ithResult != NO_ERROR) {
							EditPrintf(hwndEdit, TEXT("=== win socket start up fail ===\r\n"));
							return 1;
						}

						// ���� socket
						SOCKET m_socket;
						m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
						if (m_socket == INVALID_SOCKET) {
							EditPrintf(hwndEdit, TEXT("=== creat socket fail ===\r\n"));
							WSACleanup();
							return FALSE;
						}

						// �趨 ras server ��ַ�˿���Ϣ
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

						// ע��ͬ���¼���event��
						err = WSAAsyncSelect(m_socket, hwnd, WM_SOCKET_NOTIFY_SERVER, FD_CLOSE | FD_READ | FD_WRITE);
						if ( err == SOCKET_ERROR ) {
							EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
							closesocket(m_socket);
							WSACleanup();
							return TRUE;
						}
						// ���ӵ� ras ������
						connect(m_socket, (SOCKADDR *)&client_sin, sizeof(client_sin));
						EditPrintf(hwndEdit, TEXT("=== connection build with server ===\r\n"));

						// ��ʼ�� client job
						singleClient *singleClie;
						singleClie = globalClientHandler.findClient(*it);
						if (singleClie == NULL); // û����� client
						else {
							singleClie->init(*it);
							Client *rasClient;
							rasClient = singleClie->getANewJob();
							// UserNum ��ȥ 1 ֮������� HTML ����ǩ�� id ���Թҹ�
							rasClient->init(serverIP, serverPort, serverFile, singleClie->currentUserNum - 1, m_socket);
							EditPrintf(hwndEdit, TEXT("=== successful init a client ===\r\n"));
						}

					} // ������ vDecodeEnv.size() �ĵ�������ʾһ�������ʼ������

					if (isInvalid == true) { 
						// ��Ч������
						delete[] requestBuff;
						continue;
					}
					singleClient *singleClie;
					singleClie = globalClientHandler.findClient(*it);
					// ������Ӧͷ
					string response = string("HTTP/1.1 200 OK\r\n");
					response += string("response head\r\n");
					response += string("\r\n");
					send(singleClie->serverSocketFD, response.c_str(), response.size(), 0);
					// ���� html ��ǩ
					string htmlpage = string("<!DOCTYPE html>");;
					htmlpage += string("<html>")
						+ string("<head>")
						+ string("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />")
						+ string("<title>Network Programming Homework 3</title>")
						+ string("</head>")
						+ string("<body bgcolor=#336699>")
						+ string("<font face=\"Courier New\" size=2 color=#FFFF99>")
						+ string("<table width=\"800\" border=\"1\">")
						+ string("<tr>");
					// html ����ǩ
					for(int i = 0; i < singleClie->getUserNum(); i++) {
						Client *rasClient;
						rasClient = singleClie->getAnExisJob(i);
						if(rasClient->IP == "") continue;
						else htmlpage += string("<td>") + rasClient->IP + string("</td>");
					}
					htmlpage += string("</tr></tr>");
					// ��ǩ id ���ԣ�ʹ�� javascript �������
					for(int i = 0; i < singleClie->getUserNum(); i++) {
						Client *rasClient;
						rasClient = singleClie->getAnExisJob(i);
						if(rasClient->Port == "") continue;
						else 
							htmlpage += string("<td valign=\"top\" id=\"m") + string(to_string((unsigned long long)i)) + string("\"></td>");
					}
					string webpage2;
					webpage2 += string("</tr></table>\n");
					// ����������� html css ��ǩ
					send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(),0);
					send(singleClie->serverSocketFD, webpage2.c_str(), webpage2.size(),0);

					delete[] requestBuff;
				}	// ������ Socks.size() �ĵ���
			}

			break;
		case FD_WRITE:
			//Write your code for write event here

			break;
		case FD_CLOSE:
			break;
		};
		break;  // ���� WM_SOCKET_NOTIFY: �¼�

	case WM_SOCKET_NOTIFY_SERVER:
		switch( WSAGETSELECTEVENT(lParam)) {
		case FD_READ:
			// vClient.size() ��ʾ�������������
			for (int k = 0; k < globalClientHandler.vClients.size(); k++) {
				singleClient *singleClie;
				singleClie = &(globalClientHandler.vClients[k]);

				// userNum ��ʾ ras ����
				for (int j = 0; j < singleClie->getUserNum(); j++) {
					Client *rasClient;
					rasClient = singleClie->getAnExisJob(j);
					// �� ras ��������
					rasClient->reciveFromServ();
					string htmlpage = rasClient->returnRsltHtml() + string("\n");
					// ����������� �� ras ���ܵ�������
					send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(), 0);

					if (rasClient->isSend && rasClient->isExit) 
						continue;
					// ������ ras ���ʹ��ļ���ȡ��ָ��
					if (rasClient->isSend)
						rasClient->sendToServ();
					Sleep(1000);
					// ????
					if(rasClient->isSend && rasClient->isExit) continue;

					if (rasClient->isExit) {
						Sleep(3000);
						// �� ras ��ȡ���һ���뿪��Ϣ����ͬ exit ָ��һ���͸������
						rasClient->reciveFromServ();
						htmlpage = rasClient->returnRsltHtml() + string("\n");
						send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(), 0);
						//continue;
					}
					else {
						// ������������ļ�ָ��
						htmlpage = rasClient->returnRsltHtml() + string("\n");
						send(singleClie->serverSocketFD, htmlpage.c_str(), htmlpage.size(), 0);
						rasClient->isSend = FALSE;
					}
				}

				// ����Ƿ��������� HTML 
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
		break; // ���� WM_SOCKET_NOTIFY_SERVER: �¼�

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