#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <windows.h>

using namespace std;

// �������Ϊ ras �Ŀͻ�����Ҫ���� cgi ������
// 1. ����� query_string �н����õ��� ip port batfile
// 2. ������ ras �����ӣ���Ϊ ras �� client
// 3. �������ݣ��������ݣ����� html ��ǩ��װ���������
// ������ part 1 cgi �� linux �����µ� client ��
class Client{
public:
	string IP;
	string Port;
	string inputFile;
	string batFile;
	int taskId;
	int sockfd;
	string resultHtml;
	streampos m_lastFilePosition;
	bool isSend;
	bool isExit;

	std::string substitute(std::string str, const std::string& src, const std::string& dest) {
		size_t start = 0;
		while ((start = str.find(src, start)) != std::string::npos) {
			str.replace(start, src.length(), dest);
			start += dest.length();
		}
		return str;
	}
	// ���ַ������� javascript ��ǩ
	void Client::saveToResult(string addStr) {
		while(addStr != "" && addStr[addStr.size() - 1] == 13) 
			addStr.pop_back();

		resultHtml += "<script>document.all['m";
		resultHtml += to_string(taskId);
		resultHtml += "'].innerHTML += \"";
		resultHtml += addStr;
		resultHtml += "<br>\";</script>\n";
	}

	void Client::init(string ip, string port, string file, int id) {
		IP = ip;
		Port = port;
		resultHtml = "";
		taskId = id;
		inputFile = file;
		m_lastFilePosition = 0;
		isSend = FALSE;
		isExit = FALSE;
	
	}

	string Client::returnRsltHtml() {
		string retrunStr = resultHtml;
		resultHtml = "";
		return retrunStr;
	}

	void Client::reciveFromServ() {
		char buff[4096];
		memset(buff, 0, sizeof(char) * 4096);

		if (recv(sockfd, buff, 4096, 0) > 0) {
			string recvStr(buff);
			// ��server ȡ�������������� % ֤����������server ������Ϣ��
			if (recvStr.find('%') != string::npos) isSend = true;

			stringstream ss(recvStr);
			string tempStr;

			// �Ի���Ϊ�ָ���
			while (getline(ss, tempStr, '\n')) {
				//tempStr = substitute(tempStr, "\"", "&quot");
				tempStr = substitute(tempStr, ">", "&gt");
				tempStr = substitute(tempStr, "<", "&lt");
				tempStr = substitute(tempStr, "%", "");
				tempStr = substitute(tempStr, "\r", "");
				tempStr = substitute(tempStr, "\n", "");

				if (tempStr != " ") {
					// ʹ�� javascript ������������
					saveToResult(tempStr);
				}
			}

		}
	}

	void Client::sendToServ() {
		string cmd = "";

		ifstream batchFile(batFile.c_str());

		batchFile.seekg(m_lastFilePosition);
		if (getline(batchFile, cmd)) {
			m_lastFilePosition += cmd.size() + 1;

			for (int i = cmd.size() - 1; i > 0; i--)
				if (cmd[i] == 13) cmd.pop_back();

			// �������ô�batFile��ȡ��ָ��
			string wrapCmd = "% ";
			wrapCmd += "<b>";
			wrapCmd += cmd;
			wrapCmd += "</b>";
			saveToResult(wrapCmd);
			// �ն˲鿴��ȡ���ļ���Ϣ
			cerr << taskId << " " << cmd << " " << cmd.size() << endl;
			cmd += '\n';
			// ��ָ��ͨ��socket ���͵�server
			// ע���ڣ�linux ������ send Ҫ���� write
			send(sockfd, cmd.c_str(), cmd.size(), 0);

			if (cmd.find("exit") != string::npos) isExit = FALSE;
			else 
				isExit = true;
		}
		else {
			AllocConsole();
			freopen("CONOUT$", "w", stderr);
			cerr << "No such batfile: " << batFile << endl;
		}
	}
};


// ������װ�� ras �ͻ��� Client �� userNum
// ��Ϊ������� ras ���νӲ���
// ӳ����һ��������������� ras 
class singleClient{
public:
	singleClient(){
		currentUserNum = 0;
		serverSocketFD = 0;
	}
	~singleClient(){;}

	void init(SOCKET server){
		serverSocketFD = server;
	}

	Client* getANewJob(){
		int lastUserNum = currentUserNum;
		currentUserNum ++ ;
		return &aClientJobs[lastUserNum];
	}

	Client* getAnExisJob(int i){

		return &aClientJobs[i];
	}

	int getUserNum(){
		return currentUserNum;
	}

	// ��Ϊ������� server socket
	SOCKET serverSocketFD;
	int currentUserNum;
	Client aClientJobs[5];
	bool isActive;
};


// �������Ҫ��������� singleClient vector
class clientHandler{
public:

	void addNewClient(SOCKET serverfd){
		singleClient sC;
		sC.init(serverfd);
		vClients.push_back(sC);
	}

	void deleteClient(SOCKET serverfd){

		int IDtoErase = -1;
		for(int i = 0; i < vClients.size(); i++){
			if(vClients[i].serverSocketFD == serverfd){
				IDtoErase = i;
				break;
			}
		}

		if(IDtoErase == -1);
		else{
			vClients.erase(vClients.begin() + IDtoErase);
		}
	}

	singleClient* findClient(SOCKET serverfd){
		for(int i = 0; i < vClients.size(); i++){
			if(vClients[i].serverSocketFD == serverfd)
				return &vClients[i];
		}
		return NULL;
	}

	bool isClientExist(SOCKET serverfd){
		for(int i = 0; i < vClients.size(); i++){
			if(vClients[i].serverSocketFD == serverfd)
				return true;
		}
		return false;
	}
	vector<singleClient> vClients;
};