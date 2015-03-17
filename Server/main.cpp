#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <sstream>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <sys/wait.h>
#include <errno.h>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>



#define ERROR -1
#define MAX_CLIENTS_ALLOWED 30
#define BUFFER_SIZE 5000



using namespace std;



bool comparingCstringWithCppString(char cString[], string cppString);
bool checkingIfMessageIsComplete(int numberOfBytesReceived, char* input);
bool isDigit(string input);
void waitForPressingKey();
int getRequestOptions(string & clientRequest, string & fileName);
string getContentTypeHeader(string fileName);
string getContentLengthHeader(long fileLength);
string getHeaders(string clientRequest);
void* handleHttpRequest(void*);
void initializeThreads(vector<pthread_t> &threadVector);
void getArgumentsFromCommandLine(char* argv[]);
void creatingServerSocket(int & serverSocket);
void setSocketOptions(int & serverSocket);
void fillingUpSocketInformation(struct sockaddr_in & serverInfo);
void settingUpBind(int & serverBind, int & serverSocket, struct sockaddr_in & serverInfo, int & sizeOfSocketStruct);
void settingUpListen(int & serverListen, int &serverSocket);
void settingUpAccept(int & clientSocket, int & serverSocket, struct sockaddr_in & clientInfo, int & sizeOfSockaddr_in);
void checkingIfQueueHasWaitingConnections(int & clientSocket);
bool receivingRequest(string & clientRequest, int & clientSocket, char* inputCString, int & numberOfBytesReceived);
void analysingRequest(string & clientRequest, int & clientSocket);
void computingRequest(int & clientSocket, string & urlAndQueryString, string & clientRequest, int requestMethod);
int analysingUrl(string & fileName, DIR* &requestedDir, FILE* &requestedFile);
void sendingDirectory(DIR* &requestedDir, string &fileName, int &clientSocket);
void makingHtmlForDirectory(string &serverResponse, DIR* &requestedDir, string &fileName);
void sendingResponse(string &serverResponse, int &clientSocket);
void sendingNotFound(int &clientSocket, string &fileName);
void makingHtmlForNotFound(string &serverResponse, string &fileName);
void sendingFile(FILE* &requestedFile, string &fileName, int &clientSocket);
void makingHtmlForFile(string &serverResponse, string &fileName, long &fileLength);
void sendingResponse(char* &binaryObject, int &clientSocket, long &fileLength);
void gettingBinaryObjectFromFile(FILE* &requestedFile, char* &byteObjectBuffer, long &fileLength);
int checkIfRequestHasCgi(string pathAndFileName);
void runningCgi(string folderName, string urlAndQueryString, string clientRequest, int clientSocket, int requestMethod);
bool stopReadingFromPipe(char input[], int numberOfBytesSent);
string preparingCgiMessage(string message);
string readingFromChildCout(pid_t &pid, int childNewCout[]);
void settingUpChildCout(int childNewCout[], int &dupOutStatus);
void settingUpChildCin(int childNewCin[], int &dupInStatus);
void sendingError();
void closingClientSocket(int &clientSocket);
void fillingUpArgumentsForCgi(vector<char*> &args, vector<string> &holdsTheStringsForArgs, string &currentFolder, string &fullUrl);
void fillingUpEnviromentVariablesForCgi(vector<char*> &env, vector<string> &holdsTheStringsForEnv, string clientRequest);
void showStringVector(string name, vector<string> vect);
void showCharVector(string name, vector<char*> &vect);
vector<string> preparingNamesVector(vector<string> names);
char changingChar(char letter);
vector<string> mergingNamesVectors(vector<string> main, vector<string> secundary);
void mappingCharStarsToStrings(vector<char*> &env, vector<string> &names);
vector<string> mergingNameValuePairs(vector<string> names, vector<string> values);
bool findEndOfStream(char input[]);
char toMin(char capital);
bool noHTTPWords(string word);
void writingPostQueryToChildCin(int childNewCin[], string postQueryString);
string lookForPostQueryString(string clientRequest);
void checkIfPathIsRelativeOrAbsolute(string path);
string combinePaths(string absolutePath, string relativePath);
string reverseWord(string wordReverse);

sem_t semaphore;
queue<int> clientWaitingList;

int port = 0;
int threads = 0;
string url = "";
bool debugMode = true;
bool requestHeadersMode = false;
bool responseHeadersMode = false;

const int GET = 0;
const int POST = 1;

const int NO_FILE_OR_DIR = 0;
const int IT_IS_FILE = 1;
const int IT_IS_DIR = 2;
const int IT_IS_CGI = 3;

const int CHILD_FORK_PROCESS = 0;
const int PARENT_FORK_PROCESS = 1;

const int READ_SIDE = 0;
const int WRITE_SIDE = 1;

const int STD_IN = 0;
const int STD_OUT = 1;


int main(int argc, char* argv[])
{

    getArgumentsFromCommandLine(argv);

    vector<pthread_t> threadVector;
    initializeThreads(threadVector);

    int serverSocket=0;
    creatingServerSocket(serverSocket);

    setSocketOptions(serverSocket);

    struct sockaddr_in serverInfo;
    fillingUpSocketInformation(serverInfo);

    int serverBind = 0;
    int sizeOfSockaddr_in = sizeof(sockaddr_in);
    settingUpBind(serverBind, serverSocket, serverInfo, sizeOfSockaddr_in);

    int serverListen = 0;
    settingUpListen(serverListen, serverSocket);

    while(true) {

        struct sockaddr_in clientInfo;
        int clientSocket = 0;
        settingUpAccept(clientSocket, serverSocket, clientInfo, sizeOfSockaddr_in);

        sem_wait(&semaphore);
        clientWaitingList.push(clientSocket);
        sem_post(&semaphore);

    }
}



void* handleHttpRequest(void*) {
    void* nothing;
    while(true) {

        int clientSocket = NULL;
        checkingIfQueueHasWaitingConnections(clientSocket);

        if (clientSocket != NULL) {

            bool goodRequest = true;
            string clientRequest = "";
            int numberOfBytesReceived = BUFFER_SIZE + 1;
            char inputBuffer[BUFFER_SIZE] = "";
            goodRequest = receivingRequest(clientRequest, clientSocket, inputBuffer, numberOfBytesReceived);

            if (goodRequest == true) {
                analysingRequest(clientRequest, clientSocket);
            }
        }
    }
    return nothing;
}



void analysingRequest(string & clientRequest, int & clientSocket) {

    string fileName;
    int kindOfRequest = 0;
    kindOfRequest = getRequestOptions(clientRequest, fileName);

    if (kindOfRequest == GET)
    {
        computingRequest(clientSocket, fileName, clientRequest, GET);

    } else if (kindOfRequest == POST) {

        computingRequest(clientSocket, fileName, clientRequest, POST);

    }
}



void checkIfPathIsRelativeOrAbsolute(string path) {

    if (path[0] == '.' && path[1] == '.' && path[2] == '/')  {
        if (debugMode) cout << "path is relative ../ and it will go to parent directory" << endl;
    } else if (path[0] == '.' && path[1] == '/') {
        if (debugMode) cout << "path is relative ./ and it will go to current directory" << endl;
    } else if (path[0] == '/') {
        if (debugMode) cout << "path is absolute" << endl;
    } else if (path[0] != '.' && path[0] != '/') {
        if (debugMode) cout << "path is relative and it will go to current directory" << endl;
    }

    if (path[path.length()-1] == '/') {
        path = path.substr(0, path.length()-1);
    }

    url = path;
    //char resolvedPath[200];
    //realpath("./", resolvedPath); // this function needs a dot before the first slash to give absolute path
    //url = combinePaths(resolvedPath, path);
    //if(debugMode) cout << "relative path " << path << endl;
    //if(debugMode) cout << "absolute path " << url << endl;
}



string combinePaths(string absolutePath, string relativePath) {
    string wordAfterDotSlash;
    for (int i = 0; i < relativePath.length() ; i++) {
        if (relativePath[i] == '.' && relativePath[i+1] == '/') {
            i++;
            i++;
            for (i ; i < relativePath.length() ; i++) {
                if (relativePath[i] != '/' && i < relativePath.length()) {
                    wordAfterDotSlash = wordAfterDotSlash + relativePath[i];
                } else {
                    i = relativePath.length();
                }
            }
        }
    }

    string wordReverse;
    string wordBetweenSlashes;
    string newRealPath;
    for (int a = absolutePath.length()-1 ; a >= 0 ; a--) {
        if (absolutePath[a] != '/') {
            wordReverse = wordReverse + absolutePath[a];
        } else if (absolutePath[a] == '/'){
            wordBetweenSlashes = reverseWord(wordReverse);
            if (wordBetweenSlashes == wordAfterDotSlash) {
                newRealPath = absolutePath.substr(0, a) + "/" + relativePath.substr(2, relativePath.length());
                a = 0;
            }
            wordReverse.clear();
            wordBetweenSlashes.clear();
        }
    }

    if (newRealPath.length() == 0) {
        newRealPath = absolutePath + "/" + relativePath.substr(2, relativePath.length());
    }

    return newRealPath;
}


string reverseWord(string wordReverse) {
    string output;
    for (int i = wordReverse.length()-1 ; i >= 0 ; i--) {
        output = output + wordReverse[i];
    }
    return output;
}



void computingRequest(int & clientSocket, string & urlAndQueryString, string & clientRequest, int requestMethod) {

    string folderName = url;
    string folderAndUrlAndQueryString = folderName + urlAndQueryString;
    FILE* requestedFile;
    DIR* requestedDir;
    int kindOfFile = analysingUrl(folderAndUrlAndQueryString, requestedDir, requestedFile);

    if (kindOfFile == IT_IS_CGI) {

        runningCgi(folderName, urlAndQueryString, clientRequest, clientSocket, requestMethod);

    } else if (kindOfFile == IT_IS_DIR) {

        sendingDirectory(requestedDir, folderAndUrlAndQueryString, clientSocket);

    } else if (kindOfFile == IT_IS_FILE) {

        sendingFile(requestedFile, folderAndUrlAndQueryString, clientSocket);

    } else {

        sendingNotFound(clientSocket, folderAndUrlAndQueryString);
    }

    closingClientSocket(clientSocket);
}


void closingClientSocket(int &clientSocket) {
    if (debugMode) cout << "closing client socket" << endl;
    int closingServerSocket = close(clientSocket);
    if (closingServerSocket == ERROR) {
        cout << "error closing" << endl;
        waitForPressingKey();
    }
}



void runningCgi(string folderName, string urlAndQueryString, string clientRequest, int clientSocket, int requestMethod) {

    vector<char*> args;
    vector<string> holdsTheStringsForArgs;
    fillingUpArgumentsForCgi(args, holdsTheStringsForArgs, folderName, urlAndQueryString);

    vector<char*> env;
    vector<string> holdsTheStringsForEnv;
    fillingUpEnviromentVariablesForCgi(env, holdsTheStringsForEnv, clientRequest);

    string postQueryString;
    if (requestMethod == POST) postQueryString = lookForPostQueryString(clientRequest);

    int childNewCout[2]; // index[0] is for reading, index[1] is for writing
    int pipeOutStatus = pipe(childNewCout);

    int childNewCin[2];
    int pipeInStatus = pipe(childNewCin);

    if (pipeOutStatus != ERROR && pipeInStatus != ERROR) {
        pid_t pid = fork();

        if (pid == CHILD_FORK_PROCESS) {

            int dupOutStatus = 0;
            int dupInStatus = 0;

            settingUpChildCout(childNewCout, dupOutStatus);
            settingUpChildCin(childNewCin, dupInStatus);

            if (dupOutStatus != ERROR && dupInStatus != ERROR) {

                execve (args[0], &args[0], &env[0]); // this function solves relative and absolute paths!!
                sendingError();

            } else {
                cout << "error in dup" << endl;
                sendingError();
            }

        } else if (pid == ERROR) {

            if (debugMode) cout << "error in fork" << endl; return;

        } else {

            if (requestMethod == POST) writingPostQueryToChildCin(childNewCin, postQueryString);

            string message = readingFromChildCout(pid, childNewCout);

            message = preparingCgiMessage(message);

            sendingResponse(message, clientSocket);
        }
    } else {
        cout << "error on pipes()" << endl;
        perror("pipes");
    }

}



string lookForPostQueryString(string clientRequest) {
    string postQueryString;
    for (int i = 0 ; i < clientRequest.length() ;i++) {
        if (clientRequest[i] == '\r' && clientRequest[i+1] == '\n' && clientRequest[i+2] == '\r' && clientRequest[i+3] == '\n') {
            for (int a = i+4 ; a < clientRequest.length() ; a++) {
                postQueryString = postQueryString + clientRequest[a];
            }
        }
    }
    return postQueryString;
}



void fillingUpEnviromentVariablesForCgi(vector<char*> &env, vector<string> &holdsTheStringsForEnv, string clientRequest) {

    vector<string> names;
    vector<string> values;
    for (int a = 0 ; a < clientRequest.length() ; a++) {
        if (clientRequest[a] == 'G' && clientRequest[a+1] == 'E' && clientRequest[a+2] == 'T') {
            names.push_back("REQUEST_METHOD");
            values.push_back("GET");
            clientRequest = clientRequest.substr(3, clientRequest.length());
            break;
        } else if (clientRequest[a] == 'P' && clientRequest[a+1] == 'O' && clientRequest[a+2] == 'S' && clientRequest[a+3] == 'T') {
            names.push_back("REQUEST_METHOD");
            values.push_back("POST");
            clientRequest = clientRequest.substr(4, clientRequest.length());
            break;
        }
    }


    string fileName;
    for (int i = 0 ; i < clientRequest.length() ; i++) {
        if (!isspace(clientRequest[i])) {
            for (i ; i < clientRequest.length() ; i++) {
                if (clientRequest[i] == ' ' || clientRequest[i] == '?') {
                    i = clientRequest.length();
                } else {
                    fileName = fileName + clientRequest[i];
                }
            }
        }
    }

    //if (debugMode) cout << "after method\n\n "  << clientRequest << endl;

    string queryString;
    bool thereIsQueryString = false;
    for (int i = 0 ; i < clientRequest.length() ; i++) {
        if (clientRequest[i] == '?') {
            i++;
            thereIsQueryString = true;
            for (i ; i < clientRequest.length() ; i++) {
                if (clientRequest[i] != ' ') {
                    queryString = queryString + clientRequest[i];
                } else {
                    names.push_back("QUERY_STRING");
                    values.push_back(queryString);
                    clientRequest = clientRequest.substr(i+1, clientRequest.length());
                    i = clientRequest.length();
                }
            }
        }
    }

    if (thereIsQueryString == false) {
        for (int a = 0 ; a < clientRequest.length() ;a++) {
            if (clientRequest[a] == 'H' && clientRequest[a+1] == 'T' && clientRequest[a+2] == 'T' && clientRequest[a+3] == 'P') {
                clientRequest = clientRequest.substr(a, clientRequest.length());
                break;
            }
        }
    }

    //if (debugMode) cout << "after query string\n\n "  << clientRequest << endl;

    string protocol;
    for (int i = 0 ; i < clientRequest.length() ; i++) {
        if (clientRequest[i] != ' ') {
            for (i ; i < clientRequest.length() ; i++) {
                if (clientRequest[i] != '\r' && clientRequest[i] != '\n') {
                    protocol = protocol + clientRequest[i];
                } else if (clientRequest[i] == '\r'){
                    names.push_back("SERVER_PROTOCOL");
                    values.push_back(protocol);
                    clientRequest = clientRequest.substr(i+2, clientRequest.length());
                    i = clientRequest.length();
                } else if (clientRequest[i] == '\n') {
                    names.push_back("SERVER_PROTOCOL");
                    values.push_back(protocol);
                    clientRequest = clientRequest.substr(i+1, clientRequest.length());
                    i = clientRequest.length();
                }
            }
        }
    }

    //if (debugMode) cout << "after serverProtocol\n\n "  << clientRequest << endl;

    vector<string> pairs;
    string line;
    for (int i = 0 ; i < clientRequest.length() ; i++) {
        if (clientRequest[i] == '\r' && clientRequest[i+1] == '\n' && clientRequest[i+2] == '\r' && clientRequest[i+3] == '\n') {
            break;
        } else if (clientRequest[i] == '\r' && clientRequest[i+1] == '\n') {
            pairs.push_back(line);
            line.clear();
            i++;
        } else if (clientRequest[i] == '\n') {
            pairs.push_back(line);
            line.clear();
        } else {
            line = line + clientRequest[i];
        }
    }

    string host;
    string port;
    for (int i = 0 ; i < pairs.size() ; i++) {
        if (pairs[i].find("host:") || pairs[i].find("Host:")) {
            if (pairs[i].find("local")) {
                host = "localhost";
                names.push_back("SERVER_NAME");
                values.push_back(host);
                for (int a = 0 ; a < pairs[i].length() ; a++) {
                    if (pairs[i][a] == ':') {
                            a++;
                       for (a ; a < pairs[i].length() ; a++) {
                            if (pairs[i][a] == ':') {
                                a++;
                                for (a ; a < pairs[i].length() ; a++) {
                                    port = port + pairs[i][a];
                                }
                                names.push_back("SERVER_PORT");
                                values.push_back(port);
                                pairs.erase(pairs.begin()+i);
                                i = pairs.size();
                            }
                       }
                    }
                }
            } else {
                 for (int a = 0 ; a < pairs[i].length() ; a++) {
                    if (pairs[i][a] == ':') {
                        names.push_back("REMOTE_HOST");
                        values.push_back(pairs[i].substr(0, a));
                        names.push_back("REMOTE_PORT");
                        values.push_back(pairs[i].substr(a+1, pairs[i].length()));
                        pairs.erase(pairs.begin()+a);
                    }
                }
            }
        }
    }


    vector<string> rawNames;
    for (int i = 0 ; i < pairs.size() ; i++) {
        for (int a = 0 ; a < pairs[i].length() ; a++) {
            if (pairs[i][a] == ':') {
                if (!(pairs[i][a-4] == 'h' && pairs[i][a-3] == 't' && pairs[i][a-2] == 't' && pairs[i][a-1] == 'p'))
                {
                    if (!isdigit(pairs[i][a+1]))
                    {
                        string name = pairs[i].substr(0,a);
                        string value = pairs[i].substr(a+2, pairs[i].length());
                        rawNames.push_back(name);
                        values.push_back(value);
                    }
                }
            }
        }

    }

    rawNames = preparingNamesVector(rawNames);

    names = mergingNamesVectors(names, rawNames);

    names.push_back("SERVER_SOFTWARE");
    values.push_back("My Server");
    names.push_back("GATEWAY_INTERFACE");
    values.push_back("CGI/1.1");
    names.push_back("PATH_INFO");
    values.push_back(fileName);
    names.push_back("PATH_TRANSLATED");
    values.push_back(url + fileName);
    names.push_back("SCRIPT_NAME");
    values.push_back(fileName);
    names.push_back("REMOTE_HOST");
    values.push_back("localhost");

    holdsTheStringsForEnv = mergingNameValuePairs(names, values);

    mappingCharStarsToStrings(env, holdsTheStringsForEnv);

    if (debugMode) showStringVector("env vector ", holdsTheStringsForEnv);
    if (debugMode) showCharVector("env array ", env);
}



vector<string> mergingNameValuePairs(vector<string> names, vector<string> values) {
    vector<string> pairs;
    for (int a = 0 ; a < names.size() ; a++) {
        pairs.push_back(names[a] + "=" + values[a]);
    }
    return pairs;
}


void mappingCharStarsToStrings(vector<char*> &env, vector<string> &names) {
    for (int a = 0 ; a < names.size() ; a++) {
        env.push_back(&names[a][0]);
    }
    env.push_back(NULL);
}


vector<string> mergingNamesVectors(vector<string> main, vector<string> secundary) {
    for (int i = 0 ; i < secundary.size() ; i++) {
        main.push_back(secundary[i]);
    }
    return main;
}


vector<string> preparingNamesVector(vector<string> names) {
    vector<string> output;
    for (int i = 0 ; i < names.size() ; i++) {
        string newValue = "";
        if (!noHTTPWords(names[i])) {
            newValue= "HTTP_";
        }
        for (int a = 0 ; a < names[i].length() ; a++) {
            newValue = newValue + changingChar(names[i][a]);
        }
        output.push_back(newValue);
    }
    return output;
}



bool noHTTPWords(string word) {
    string newWord;
    for (int a = 0 ; a < word.length() ; a++) {
        newWord = newWord + toMin(word[a]);
    }

    if (newWord == "content-type") {
        return true;
    } else if (newWord == "content-length") {
        return true;
    }
    return false;
}


char toMin(char capital) {
    switch(capital) {
        case 'A' : return 'a';
        case 'B' : return 'b';
        case 'C' : return 'c';
        case 'D' : return 'd';
        case 'E' : return 'e';
        case 'F' : return 'f';
        case 'G' : return 'g';
        case 'H' : return 'h';
        case 'I' : return 'i';
        case 'J' : return 'j';
        case 'K' : return 'k';
        case 'L' : return 'l';
        case 'M' : return 'm';
        case 'N' : return 'n';
        case 'O' : return 'o';
        case 'P' : return 'p';
        case 'Q' : return 'q';
        case 'R' : return 'r';
        case 'S' : return 's';
        case 'T' : return 't';
        case 'U' : return 'u';
        case 'V' : return 'v';
        case 'W' : return 'w';
        case 'X' : return 'x';
        case 'Y' : return 'y';
        case 'Z' : return 'z';
    }
    return capital;
}


char changingChar(char letter) {
    switch(letter) {
        case 'a' : return 'A';
        case 'b' : return 'B';
        case 'c' : return 'C';
        case 'd' : return 'D';
        case 'e' : return 'E';
        case 'f' : return 'F';
        case 'g' : return 'G';
        case 'h' : return 'H';
        case 'i' : return 'I';
        case 'j' : return 'J';
        case 'k' : return 'K';
        case 'l' : return 'L';
        case 'm' : return 'M';
        case 'n' : return 'N';
        case 'o' : return 'O';
        case 'p' : return 'P';
        case 'q' : return 'Q';
        case 'r' : return 'R';
        case 's' : return 'S';
        case 't' : return 'T';
        case 'u' : return 'U';
        case 'v' : return 'V';
        case 'w' : return 'W';
        case 'x' : return 'X';
        case 'y' : return 'Y';
        case 'z' : return 'Z';
        case '-' : return '_';
    }
    return letter;
}


void showStringVector(string name, vector<string> vect) {
    for (int a = 0 ; a < vect.size() ; a++) {
        cout << name << vect[a] << endl;
    }
}

void showCharVector(string name, vector<char*> &vect) {
    for (int a = 0 ; a < vect.size() ; a++) {
        if (vect[a] != NULL) {
            cout << name << vect[a] << endl;
        }
    }
}


void fillingUpArgumentsForCgi(vector<char*> &args, vector<string> &holdsTheStringsForArgs, string &currentFolder, string &fullUrl) {

    string cgiFileName;
    string nameValuePairs;
    for (int i = 0 ; i < fullUrl.length()-2 ; i++) {
        if (fullUrl[i] == '.' && fullUrl[i+1] == 'p' && fullUrl[i+2] == 'y') {
            cgiFileName = cgiFileName + ".py";
            break;
        } else if (fullUrl[i] == '.' && fullUrl[i+1] == 'c' && fullUrl[i+2] == 'g' && fullUrl[i+3] == 'i') {
            cgiFileName = cgiFileName + ".cgi";
            break;
        } else {
            cgiFileName = cgiFileName + fullUrl[i];
        }
    }

    string cgiAddressFile = currentFolder + cgiFileName;
    holdsTheStringsForArgs.push_back(cgiAddressFile);

    mappingCharStarsToStrings(args, holdsTheStringsForArgs);

    if (debugMode) showStringVector("args vector ", holdsTheStringsForArgs);
    if (debugMode) showCharVector("args array ", args);
}


void sendingError() {
    cout << "ERROR" << endl;
    exit(1);
}



void settingUpChildCout(int childNewCout[], int &dupOutStatus) {

    dupOutStatus = dup2(childNewCout[WRITE_SIDE], STD_OUT);

    close(childNewCout[READ_SIDE]);
    close(childNewCout[WRITE_SIDE]);
}



void settingUpChildCin(int childNewCin[], int &dupInStatus) {

    dupInStatus = dup2(childNewCin[READ_SIDE], STD_IN);

    close(childNewCin[READ_SIDE]);
    close(childNewCin[WRITE_SIDE]);
}



void writingPostQueryToChildCin(int childNewCin[], string postQueryString) {
    char serverResponse[postQueryString.length()+1];
   strcpy(serverResponse, postQueryString.c_str());
    int clientSocket = childNewCin[WRITE_SIDE];
    if (debugMode) cout << "writing to child: " << serverResponse << endl;

        int sizeOfResponse = postQueryString.length();
        int sizeOfResponseRemainder = sizeOfResponse;
        int numberOfBytesSentSoFar = 0;
        int numberOfBytesSent = 1;
        while(numberOfBytesSentSoFar < sizeOfResponse) {
            if(sizeOfResponseRemainder > BUFFER_SIZE) {
                if (debugMode) cout << "sending " << BUFFER_SIZE << " bytes" << endl;
                numberOfBytesSent = write(clientSocket, &serverResponse[numberOfBytesSentSoFar], BUFFER_SIZE);
            } else {
                if (debugMode) cout << "sending " << sizeOfResponseRemainder << " bytes" << endl;
                numberOfBytesSent = write(clientSocket, &serverResponse[numberOfBytesSentSoFar], sizeOfResponseRemainder);
            }
            if (numberOfBytesSent == ERROR) {
                if (debugMode) cout << "error sending" << endl;
                break;
            }
            numberOfBytesSentSoFar = numberOfBytesSentSoFar + numberOfBytesSent;
            sizeOfResponseRemainder = sizeOfResponseRemainder - numberOfBytesSent;
        }


    close(childNewCin[READ_SIDE]);
    close(childNewCin[WRITE_SIDE]);
}


string readingFromChildCout(pid_t &pid, int childNewCout[]) {

    if (debugMode) cout << "before reading what child did" << endl;



    int waitingForChildStatus;
    waitpid(pid, &waitingForChildStatus, 0);

    if (waitingForChildStatus != ERROR) {

        if (debugMode) cout << "after reading what child did" << endl;

        string message;
        char input[BUFFER_SIZE];
        int numberOfBytesReceived = 0;

        fcntl(childNewCout[READ_SIDE], F_SETFL, O_NONBLOCK);

        do {
            numberOfBytesReceived = read(childNewCout[READ_SIDE],input,BUFFER_SIZE);
            string temp = input;
            message = message + temp;
        } while (!stopReadingFromPipe(input, numberOfBytesReceived));

        close(childNewCout[READ_SIDE]);
        close(childNewCout[WRITE_SIDE]);

        return message;
    } else {
        cout << "error waiting" << endl;
        return "ERROR WHILE WAITING";
    }
}



string preparingCgiMessage(string message) {

    string response = "HTTP/1.1 200 OK\r\n";
    if (!message.find("Content-type:")) {
        if (!message.find("Content-Type:")) {
        response = response + "Content-Type: text/html\r\n\r\n";
        }
    }
    response = response + message + "\r\n\r\n";
    if (debugMode) cout << "\nmessage: \n" << response << endl;
    return response;
}



bool stopReadingFromPipe(char input[], int numberOfBytesSent) {
    if (numberOfBytesSent == ERROR) {
        cout << "error while reading pipe" << endl;
        return true;
    } else if (numberOfBytesSent == 0) {
        cout << "read said it is the end of file" << endl;
        return true;
    } else if (input[0] == 'E' && input[1] == 'R' && input[2] == 'R' && input[3] == 'O' && input[4] == 'R') {
        cout << "file did not execute" << endl;
        return true;
    } else if (numberOfBytesSent < BUFFER_SIZE) {
        cout << "bytes sent is smaller than buffer, so it is the end" << endl;
        return true;
    } else {
        return false;
    }
}


bool findEndOfStream(char input[]) {
    for (int a = 0 ; a < BUFFER_SIZE ; a++) {
        if (input[a] == '/0' || input[a] == 0) {
            return true;
        }
    }
    return false;
}



int analysingUrl(string & pathAndFileName, DIR* &requestedDir, FILE* &requestedFile) {

    if (debugMode) cout << "analysing file or directory" << endl;

    int requestedCgi;
    char fileNameWithPath[pathAndFileName.length()+1]; strcpy(fileNameWithPath, pathAndFileName.c_str());
    const char* openFileAsBinary = "rb";

    requestedCgi = checkIfRequestHasCgi(pathAndFileName);
    requestedDir = opendir(fileNameWithPath); // it likes the slash -- /public_html/test1.html
    requestedFile = fopen(fileNameWithPath, openFileAsBinary); // it likes the slash -- /public_html/test1.html

    if (requestedCgi != NULL) {

        return IT_IS_CGI;

    } else if (requestedDir != NULL) {

        return IT_IS_DIR;

    } else if (requestedFile != NULL) {

        return IT_IS_FILE;

    } else {

        return NO_FILE_OR_DIR;

    }
}



int checkIfRequestHasCgi(string pathAndFileName) {
    for (unsigned int a = 0 ; a < pathAndFileName.length() ; a++) {
        if (pathAndFileName[a] == '.' && pathAndFileName[a+1] == 'c' && pathAndFileName[a+2] == 'g' && pathAndFileName[a+3] == 'i')
        {
            return 1;
        }
        if (pathAndFileName[a] == '.' && pathAndFileName[a+1] == 'p' && pathAndFileName[a+2] == 'y') {
            return 1;
        }
    }
    return NULL;
}



string getContentLengthHeader(long fileLength) {
    stringstream ss;
    ss << fileLength;
    return "Content-Length: " + ss.str();
}



string getContentTypeHeader(string fileName) {
    int position;
    if((position = fileName.find(".gif")) != -1) {
        return "Content-Type: image/gif";
    }
    if((position = fileName.find(".jpg")) != -1) {
        return "Content-Type: image/jpg";
    }
    if((position = fileName.find(".txt")) != -1) {
        return "Content-Type: text/plain";
    }
    if((position = fileName.find(".html")) != -1) {
        return "Content-Type: text/html";
    }
    return "it never gets here";
}



void waitForPressingKey() {
    cout << "\nPress any key..." << endl;
    cin.get();
    exit(0);
}



bool comparingCstringWithCppString(char cString[], string cppString) {
    string cStringTocppString = cString;
    if(cStringTocppString == cppString) {
        return true;
    }
    return false;
}



bool isDigit(string input) {
    for(unsigned int a = 0 ; a < input.length() ; a++) {
        if(!isdigit(input[a])) {
            return false;
        }
    }
    return true;
}



bool checkingIfMessageIsComplete(int numberOfBytesReceived, char* input) {
    if (numberOfBytesReceived == ERROR) {
        cout << "error while receiving request" << endl;
        return true;
    }
    if (numberOfBytesReceived == 0) {
        cout << "connection closed by client" << endl;
        return true;
    }
    if (numberOfBytesReceived < BUFFER_SIZE) {
        return true;
    }
    return false;
}



string getHeaders(string clientRequest) {
    int a = 0;
    string headers;
    while(clientRequest[a] != '\r' && clientRequest[a+1] != '\n' && clientRequest[a+2] != '\r' && clientRequest[a+3] != '\n') {
        if (clientRequest[a] != '\r' && clientRequest[a+1] != '\n') {
            headers = headers + clientRequest[a];
        } else {
            headers = headers + "\n";
        }
        a++;
    }
    return headers;
}



int getRequestOptions(string & clientRequest, string & fileName) {

    if (requestHeadersMode) cout << "\nrequest headers: \n" << clientRequest << endl;
    if (debugMode)          cout << "\nrequest headers: \n" << clientRequest << endl;

    if (clientRequest[0] == 'G' && clientRequest[1] == 'E' && clientRequest[2] == 'T') {
        for (unsigned int a = 3 ; a < clientRequest.length() ; a++) {
            if (!isspace(clientRequest[a])) {
                for (unsigned int b = a ; b < clientRequest.length() ; b++) {
                    if (clientRequest[b] == 'H' && clientRequest[b+1] == 'T' && clientRequest[b+2] == 'T' && clientRequest[b+3] == 'P') {
                        if (debugMode) cout << "filename is: " << fileName << endl;
                        return GET;
                    }
                    if (!isspace(clientRequest[b])) {
                        fileName = fileName + clientRequest[b];
                    }
                }
            }
        }
    }

    if (clientRequest[0] == 'P' && clientRequest[1] == 'O' && clientRequest[2] == 'S' && clientRequest[3] == 'T') {
        for (unsigned int a = 4 ; a < clientRequest.length() ; a++) {
            if (!isspace(clientRequest[a])) {
                for (unsigned int b = a ; b < clientRequest.length() ; b++) {
                    if (clientRequest[b] == 'H' && clientRequest[b+1] == 'T' && clientRequest[b+2] == 'T' && clientRequest[b+3] == 'P') {
                        if (debugMode) cout << "filename is: " << fileName << endl;
                        return POST;
                    }
                    if (!isspace(clientRequest[b])) {
                        fileName = fileName + clientRequest[b];
                    }
                }
            }
        }
    }
}



void initializeThreads(vector<pthread_t> &threadVector) {

    if (debugMode) cout << "making threads" << endl;

    for (int a = 0 ; a < threads ; a++) {
        pthread_t tempThread;
        threadVector.push_back(tempThread);
    }


    sem_init(&semaphore, 0, 1);
    for (int a = 0 ; a < threads ; a++) {
        sem_wait(&semaphore);
        pthread_create(&threadVector[ a ], NULL, handleHttpRequest, NULL);
        sem_post(&semaphore);
    }
}



void getArgumentsFromCommandLine(char* argv[]) {

    cout << "reading arguments from command line" << endl;

    if (comparingCstringWithCppString(argv[1], "-d")) {
            debugMode = false;
         if (isDigit(argv[2])) {
            port = atoi(argv[2]);
        } else {
            cout << "port must be a number" << endl;
            waitForPressingKey();
        }
        if (isDigit(argv[3])) {
            threads = atoi(argv[3]);
        } else {
            cout << "threads must be a number" << endl;
            waitForPressingKey();
        }
        checkIfPathIsRelativeOrAbsolute(argv[4]);
    } else if (comparingCstringWithCppString(argv[1], "-U")) {
        requestHeadersMode = true;
         if (isDigit(argv[2])) {
            port = atoi(argv[2]);
        } else {
            cout << "port must be a number" << endl;
            waitForPressingKey();
        }
        if (isDigit(argv[3])) {
            threads = atoi(argv[3]);
        } else {
            cout << "threads must be a number" << endl;
            waitForPressingKey();
        }
        checkIfPathIsRelativeOrAbsolute(argv[4]);
    } else if (comparingCstringWithCppString(argv[1], "-e")) {
        responseHeadersMode = true;
         if (isDigit(argv[2])) {
            port = atoi(argv[2]);
        } else {
            cout << "port must be a number" << endl;
            waitForPressingKey();
        }
        if (isDigit(argv[3])) {
            threads = atoi(argv[3]);
        } else {
            cout << "threads must be a number" << endl;
            waitForPressingKey();
        }
        checkIfPathIsRelativeOrAbsolute(argv[4]);
    } else {
        if (isDigit(argv[1])) {
            port = atoi(argv[1]);
        } else {
            cout << "port must be a number" << endl;
            waitForPressingKey();
        }
        if (isDigit(argv[2])) {
            threads = atoi(argv[2]);
        } else {
            cout << "threads must be a number" << endl;
            waitForPressingKey();
        }
        checkIfPathIsRelativeOrAbsolute(argv[3]);
    }
}



void creatingServerSocket(int & serverSocket) {

    if (debugMode) cout << "making server socket" << endl;

    serverSocket=socket(AF_INET,SOCK_STREAM,0);
    if (serverSocket == ERROR)
    {
        cout << "error creating socket" << endl;
        waitForPressingKey();
    }
}



void setSocketOptions(int & serverSocket) {

    if (debugMode) cout << "setting up socket options" << endl;

    int optval = 1;
    setsockopt (serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}



void fillingUpSocketInformation(struct sockaddr_in & serverInfo) {

    if (debugMode) cout << "filling up socket struct Information" << endl;

    serverInfo.sin_addr.s_addr=INADDR_ANY;
    serverInfo.sin_port=htons(port);
    serverInfo.sin_family=AF_INET;
}



void settingUpBind(int & serverBind, int & serverSocket, struct sockaddr_in & serverInfo, int & sizeOfSocketStruct) {

    if (debugMode) cout << "setting up bind" << endl;

    serverBind = bind(serverSocket, (struct sockaddr*)& serverInfo, sizeOfSocketStruct);
    if (serverBind == ERROR) {
        cout << "error binding" << endl;
        waitForPressingKey();
    }
}



void settingUpListen(int & serverListen, int &serverSocket) {

    if (debugMode) cout << "setting up listen" << endl;

    serverListen = listen(serverSocket, MAX_CLIENTS_ALLOWED);
    if (serverListen == ERROR) {
        cout << "error listening" << endl;
        waitForPressingKey();
    }
}



void settingUpAccept(int & clientSocket, int & serverSocket, struct sockaddr_in & clientInfo, int & sizeOfSockaddr_in) {

      if (debugMode) cout << "waiting for accept" << endl;

      clientSocket = accept(serverSocket, (struct sockaddr*)& clientInfo, (socklen_t *)&sizeOfSockaddr_in);
      if (clientSocket == ERROR) {
            cout << "error accepting" << endl;
            waitForPressingKey();
      }
}



void checkingIfQueueHasWaitingConnections(int & clientSocket) {
    sem_wait(&semaphore);
    if (clientWaitingList.size() > 0) {
        if (debugMode) cout << "queue size " << clientWaitingList.size() << endl;
        if (debugMode) cout.flush();
        clientSocket = clientWaitingList.front();
        clientWaitingList.pop();
    }
    sem_post(&semaphore);
}



bool receivingRequest(string & clientRequest, int & clientSocket, char* inputCString, int & numberOfBytesReceived) {

    if (debugMode) cout << "receiving request" << endl;

    //fcntl(clientSocket, F_SETFL, O_NONBLOCK);// this function should not be used with recv... recv and read are not the same!!!! for the second time

    while(!checkingIfMessageIsComplete(numberOfBytesReceived, inputCString)) {
        numberOfBytesReceived = read(clientSocket,inputCString,BUFFER_SIZE);////////
        string inputCppString = inputCString;
        clientRequest = clientRequest + inputCppString;
    }
    return true;
}



void sendingDirectory(DIR* &requestedDir, string &fileName, int &clientSocket) {

    if (debugMode) cout << "path is a directory" << endl;

    string serverResponse;
    makingHtmlForDirectory(serverResponse, requestedDir, fileName);

    sendingResponse(serverResponse, clientSocket);
}



void makingHtmlForDirectory(string &serverResponse, DIR* &requestedDir, string &fileName) {

    struct dirent* objectIDirectory;
    stringstream listOfFilesInDirectory;
    while ((objectIDirectory = readdir(requestedDir)) != NULL) {
        string subfileName = objectIDirectory->d_name;
        if (subfileName != "." && subfileName != "..")
        listOfFilesInDirectory << "<a href=\"http://isai-x200ma:" << port << "/" << objectIDirectory->d_name <<"\">" << objectIDirectory->d_name << "</a><br>" << endl;
    }
    closedir(requestedDir);

    if (debugMode) cout << "making response" << endl;
    string responseStatus = "HTTP/1.1 200 OK";
    string contentTypeHeader = "Content-Type: text/html";
    string connectionHeader = "Connection: close";
    string responseBodyBeforeDirectory = "<html>Directory!<body><div>";
    string listOfFiles = listOfFilesInDirectory.str();
    string responseBodyAfterDirectory = "</div></body></html>";
    serverResponse = responseStatus + "\r\n" + contentTypeHeader + "\r\n" + connectionHeader + "\r\n\r\n" + responseBodyBeforeDirectory + listOfFiles + responseBodyAfterDirectory + "\r\n\r\n";


    if (responseHeadersMode) cout << "\nresponse headers:" << endl;
    if (responseHeadersMode) cout << contentTypeHeader << endl;
    if (responseHeadersMode) cout << connectionHeader << endl;
}



void sendingResponse(string &serverResponse, int &clientSocket) {

    if (debugMode) cout << "sending Response" << endl;
    if (debugMode) cout << "response size " << serverResponse.length() << endl;

        int sizeOfResponse = serverResponse.length();
        int sizeOfResponseRemainder = sizeOfResponse;
        int numberOfBytesSentSoFar = 0;
        int numberOfBytesSent = 1;
        while(numberOfBytesSentSoFar < sizeOfResponse) {
            if(sizeOfResponseRemainder > BUFFER_SIZE) {
                numberOfBytesSent = send(clientSocket, &serverResponse.c_str()[numberOfBytesSentSoFar], BUFFER_SIZE, MSG_NOSIGNAL);// send is necesary for the MSG_NOSIGNAL flag
            } else {
                numberOfBytesSent = send(clientSocket, &serverResponse.c_str()[numberOfBytesSentSoFar], sizeOfResponseRemainder, MSG_NOSIGNAL);
            }
            if (numberOfBytesSent == ERROR) {
                if (debugMode) cout << "error sending" << endl;
                break;
            }
            numberOfBytesSentSoFar = numberOfBytesSentSoFar + numberOfBytesSent;
            sizeOfResponseRemainder = sizeOfResponseRemainder - numberOfBytesSent;
        }
}



void sendingNotFound(int &clientSocket, string &fileName) {

    if (debugMode) cout << "path is not a file nor a directory" << endl;

    string serverResponse;
    makingHtmlForNotFound(serverResponse, fileName);

    sendingResponse(serverResponse, clientSocket);
}



void makingHtmlForNotFound(string &serverResponse, string &fileName) {

    if (debugMode) cout << "making response 404" << endl;

    string responseStatus = "HTTP/1.1 404 Not Found";
    string contentTypeHeader = "Content-Type: text/html";
    string connectionHeader = "Connection: close";
    string responseBody = "<HTML><BODY><div>Error 404<br></div><br>" + fileName + "<br><div><H2>File or Directory not found</H2></div></BODY></HTML>";
    serverResponse = responseStatus + "\r\n" + contentTypeHeader + "\r\n" + connectionHeader + "\r\n\r\n" + responseBody + "\r\n\r\n";

    if (responseHeadersMode) cout << "\nresponse headers: " << endl;
    if (responseHeadersMode) cout << contentTypeHeader << endl;
    if (responseHeadersMode) cout << connectionHeader << endl;
}



void sendingFile(FILE* &requestedFile, string &fileName, int &clientSocket) {

    char* byteObjectBuffer;
    long fileLength;
    gettingBinaryObjectFromFile(requestedFile, byteObjectBuffer, fileLength);

    string serverResponseHeaders;
    makingHtmlForFile(serverResponseHeaders, fileName, fileLength);

    sendingResponse(serverResponseHeaders, clientSocket);

    sendingResponse(byteObjectBuffer, clientSocket, fileLength);
}



void makingHtmlForFile(string &serverResponse, string &fileName, long &fileLength) {
    string responseStatus = "HTTP/1.1 200 OK";
    string contentTypeHeader = getContentTypeHeader(fileName);
    string contentLengthHeader = getContentLengthHeader(fileLength);
    serverResponse = responseStatus + "\r\n" + contentTypeHeader + "\r\n" + contentLengthHeader + "\r\n\r\n";

    if (responseHeadersMode) cout << "\nresponse headers: " << endl;
    if (responseHeadersMode) cout << contentTypeHeader << endl;
    if (responseHeadersMode) cout << contentLengthHeader << endl;
}



void sendingResponse(char* &binaryObject, int &clientSocket, long &fileLength) {

    if (debugMode) cout << "sending binary data" << endl;

    int sizeOfResponse = fileLength;
    int sizeOfResponseRemainder = fileLength;
    int numberOfBytesSentSoFar = 0;
    int numberOfBytesSent = 1;
    while(numberOfBytesSentSoFar < sizeOfResponse) {
        if(sizeOfResponseRemainder > BUFFER_SIZE) {
            numberOfBytesSent = send(clientSocket, &binaryObject[numberOfBytesSentSoFar], BUFFER_SIZE, MSG_NOSIGNAL);
        } else {
            numberOfBytesSent = send(clientSocket, &binaryObject[numberOfBytesSentSoFar], sizeOfResponseRemainder, MSG_NOSIGNAL);
        }
        if (numberOfBytesSent == ERROR) {
            break;
        }
        numberOfBytesSentSoFar = numberOfBytesSentSoFar + numberOfBytesSent;
        sizeOfResponseRemainder = sizeOfResponseRemainder - numberOfBytesSent;
    }
}



void gettingBinaryObjectFromFile(FILE* &requestedFile, char* &byteObjectBuffer, long &fileLength) {

    int spacesAwayFromSEEK_END = 0;
    fseek (requestedFile, spacesAwayFromSEEK_END, SEEK_END); // Sets the file-pointer position to 0 spaces away from end, therefore, pointer is at the end
    fileLength = ftell(requestedFile); // Returns the current pointer-position. In other words since last statement put the pointer to the end, ftell() will return the length
    rewind(requestedFile); // Returns the pointer-position of the beginning of the file
    byteObjectBuffer = (char*) malloc (sizeof(char)*fileLength); // making array in heap because it is runtime. byteObjectBuffer[fileLength+1]; does not work maybe because it tries to initalize the array in memory space that is already taken
    int bytesToBeReadAtATime = 1;
    fread(byteObjectBuffer, bytesToBeReadAtATime, fileLength, requestedFile); // reads one byte at a time from requestedFile and saves it to byteObjectBuffer
    fclose(requestedFile); // closes the file
}
