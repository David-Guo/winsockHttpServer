#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <windows.h>

using namespace std;

// 这个类作为 ras 的客户，主要负责 cgi 的事务
// 1. 保存从 query_string 中解析得到的 ip port batfile
// 2. 建立与 ras 的连接，作为 ras 的 client
// 3. 输入数据，接受数据，并用 html 标签封装好输入输出
// 复用了 part 1 cgi 中 linux 环境下的 client 类
class Client{
public:
	string IP;
	string Port;
	string inputFile;
	int taskId;
	SOCKET sockfd;
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
	// 给字符串加上 javascript 标签
	void Client::saveToResult(string addStr) {
		while(addStr != "" && addStr[addStr.size() - 1] == 13) 
			addStr.pop_back();

		resultHtml += "<script>document.all['m";
		resultHtml += to_string(taskId);
		resultHtml += "'].innerHTML += \"";
		resultHtml += addStr;
		resultHtml += "<br>\";</script>\n";
	}

	void Client::init(string ip, string port, string file, int id, SOCKET sock) {
		IP = ip;
		Port = port;
		resultHtml = "";
		taskId = id;
		inputFile = file;
		sockfd = sock;
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
			// 从server 取到输出结果，遇到 % 证明，可以向server 发送信息了
			if (recvStr.find('%') != string::npos) isSend = true;

			stringstream ss(recvStr);
			string tempStr;

			// 以换行为分隔符
			while (getline(ss, tempStr, '\n')) {
				//tempStr = substitute(tempStr, "\"", "&quot");
				tempStr = substitute(tempStr, ">", "&gt");
				tempStr = substitute(tempStr, "<", "&lt");
				tempStr = substitute(tempStr, "%", "");
				tempStr = substitute(tempStr, "\r", "");
				tempStr = substitute(tempStr, "\n", "");

				if (tempStr != " ") {
					// 使用 javascript 打包好输出内容
					saveToResult(tempStr);
				}
			}

		}
		else {
			AllocConsole();
			freopen("CONOUT$", "w", stderr);
			cerr << "recvFrom server error for taskID = " << taskId << endl;
		}
	}

	void Client::sendToServ() {
		string cmd = "";

		ifstream batchFile(inputFile.c_str());

		batchFile.seekg(m_lastFilePosition);
		if (getline(batchFile, cmd)) {
			m_lastFilePosition += cmd.size() + 1;

			for (int i = cmd.size() - 1; i > 0; i--)
				if (cmd[i] == 13) cmd.pop_back();

			// 粗体打包好从batFile读取的指令
			string wrapCmd = "% ";
			wrapCmd += "<b>";
			wrapCmd += cmd;
			wrapCmd += "</b>";
			saveToResult(wrapCmd);
			// 终端查看读取的文件信息
			cerr << taskId << " " << cmd << " " << cmd.size() << endl;
			cmd += '\n';
			// 将指令通过socket 发送到server
			// 注意在，linux 环境中 send 要换成 write
			send(sockfd, cmd.c_str(), cmd.size(), 0);

			if (cmd.find("exit") != string::npos) isExit = true;
			else 
				isExit = false;
		}
		else {
			AllocConsole();
			freopen("CONOUT$", "w", stderr);
			cerr << "No such batfile: " << inputFile << endl;
		}
	}
};


// 这个类封装了 ras 客户类 Client 与 userNum
// 作为浏览器与 ras 的衔接部分
// 映射起一个浏览器请求与多个 ras 
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

	// 作为浏览器的 server socket
	SOCKET serverSocketFD;
	int currentUserNum;
	Client aClientJobs[5];
	bool isActive;
};


// 这个类主要管理浏览器 singleClient vector
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