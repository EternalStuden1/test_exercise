#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include "winsock2.h"
#include <fstream>


//данную библиотеку (libwsock32.a) я включил в линкер в своем компиляторе
#pragma comment(lib, "ws2_32.lib")


class ExceptionHandler{
private:
    std::string m_cause;

public:
    std::string getCause(){ return m_cause;}
    void operator()(std::string cause) { m_cause = cause; }
};

    struct Link{
        std::string link;
        int l_shift;
    };

bool DirExists(std::string dir);

char* OpenURL(char* url, ExceptionHandler &error) {

    WSADATA lpWSAData;
    SOCKET s;
    // Адрес должен начинаться с "http://"
    if (memcmp(url,"HTTP://",7)!=0 && memcmp(url,"http://",7)!=0){
        error("Incorrect address.");
        return(NULL);
    }
    url+=7;
    // Инициализация Winsock.
    if (FAILED ( WSAStartup(MAKEWORD(1,1),&lpWSAData))) //WSAStartup(Version_WinSock, WSDATA)
        {
        error("WSA initialization error");
        return(NULL);
        }

    // Получим имя хоста, номер порта и путь

    char *http_host=strdup(url); // Имя хоста
    int port_num=80;             // Номер порта по умолчанию
    char *http_path=NULL;        // Путь

    char *pch=strchr(http_host,':');
    if (!pch) {
        pch=strchr(http_host,'/');
        if (pch) {
            *pch=0;
            http_path=strdup(pch+1);
            }
        else http_path=strdup("");
        }
    else {
        *pch=0;
        pch++;
        char *pch1 = strchr(pch,'/');
        if (pch1) {
            *pch1=0;
            http_path=strdup(pch1+1);
            }
        else
            http_path=strdup("");

        port_num = atoi(pch);
        if (port_num == 0) port_num = 80;
        }

    // Получаем IP адрес по имени хоста
    hostent* hp;
    if (!(hp=gethostbyname(http_host))) {
        free(http_host);
        free(http_path);
        error("Error: incorrect hostname.");
        return(NULL);
        }

    // Открываем сокет
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        free(http_host);
        free(http_path);
        error("Error: enable to open socket.");
        return(NULL);
        }

    // объявим переменную для хранения адреса
    sockaddr_in soc_addr;
    memset ((char *)&soc_addr, 0, sizeof(soc_addr)); //заполним ее нулями
    soc_addr.sin_family = AF_INET; //тип адреса (TCP/IP)
    soc_addr.sin_addr.S_un.S_un_b.s_b1 = hp->h_addr[0]; //биты IP
    soc_addr.sin_addr.S_un.S_un_b.s_b2 = hp->h_addr[1];
    soc_addr.sin_addr.S_un.S_un_b.s_b3 = hp->h_addr[2];
    soc_addr.sin_addr.S_un.S_un_b.s_b4 = hp->h_addr[3];
    soc_addr.sin_port = htons(port_num); //преобразования порта в понятную для TCP форму

    // Соединяемся с хостом
    if (connect(s, (sockaddr *)&soc_addr, sizeof(soc_addr))!= 0) {
        free(http_host);
        free(http_path);
        error("Error: enable to connect.");
        return(NULL);
        }

    // Формируем HTTP запрос
    char *query=(char*)malloc(2048);

    strcpy(query,"GET /");
    strcat(query,http_path);
    strcat(query," HTTP/1.1\r\nHost: ");
    strcat(query,http_host);
    strcat(query,"\r\nConnection: close\r\n\r\n");

    // Отправляем запрос серверу
    int cnt=send(s,query,strlen(query),0);
    // Освобождаем память
    free(http_host);
    free(http_path);
    free(query);

    // Проверяем, не произошло ли ошибки при отправке запроса на сервер
     if (cnt == 1){
            error("Error sending request to the server. ");
            return(NULL);
     }

    // Получаем ответ с сервера
    //int size=1024*1024; // 1Mb
    int size=1024*1024;
    char *result=(char*)malloc(size);
    strcpy(result,"");
    char *result_ptr=result;
    cnt = 1;
    while (cnt != 0 && size > 0) {
        cnt=recv (s, result_ptr, sizeof(size),0);
        if (cnt>0) {
            result_ptr+=cnt;
            size-=cnt;
            }
        }

    delete result_ptr;
    // Деинициализация библиотеки Ws2_32.dll
    WSACleanup();

    return(result);
    }

    //функция для поиска в html-коде ссылок
    void FindLink(std::string content, std::string descriptor, Link &CurrentLink){
        CurrentLink.l_shift = content.find(descriptor, CurrentLink.l_shift);
        if (CurrentLink.l_shift != -1){
        CurrentLink.l_shift = content.find("href=", CurrentLink.l_shift);
        CurrentLink.l_shift += 6;
        CurrentLink.link.assign(content, CurrentLink.l_shift, content.find('"', CurrentLink.l_shift)-CurrentLink.l_shift);
        }
    }


    //функция для записи ссылок в файл Link_x.txt по указанному пути
    void SaveLinks(std::string content, ExceptionHandler &error, std::string path){
            std::ofstream fout;
            int counter = 1;
            Link CurrentLink;
            CurrentLink.l_shift = 0;
            while (CurrentLink.l_shift != -1){
                FindLink(content, "<a", CurrentLink);
                fout.open(std::string(path)+"Link_" + std::to_string(counter)+".txt");
                fout << CurrentLink.link;
                fout.close();
                counter++;
            }
            CurrentLink.l_shift = 0;
            while (CurrentLink.l_shift != -1){
                FindLink(content, "<link", CurrentLink);
                fout.open(std::string(path)+"Link_" + std::to_string(counter)+".txt");
                fout << CurrentLink.link;
                fout.close();
                counter++;
            }
        }

bool DirExists(std::string path)
{
  int SIZE = path.length()+1;
  char* dir = new char[SIZE];
  strcpy(dir, path.c_str());
  DWORD code = GetFileAttributes(dir);
  return code != 0xFFFFFFFF && (FILE_ATTRIBUTE_DIRECTORY & code) != 0;
}

int main() {
    setlocale (LC_ALL, "rus");
    ExceptionHandler exeptions;
    int SIZE = 100;
    char* url = new char[SIZE];
    std::cout << "Your URL: ";
    std::cin.get(url, SIZE);
    std::string path;
    std::cout << "Input path (example: C:/result/): ";
    std::cin >> path;
    char *result=OpenURL(url, exeptions);
    if (!DirExists(path)){
        exeptions("Incorrect path.");
        std::cout << exeptions.getCause() << std::endl;
        return 0;
    }
    if (result){
        path+="1";
        if (path.compare(path.length()-2, 2, "/1") != 0 || path.compare(path.length()-2, 2, "\1") != 0)
            path[path.length()-1] = '/';
        SaveLinks(result, exeptions, path);
    }
    else{
        std::cout << exeptions.getCause();
        return 0;
    }
    free(url);
    std::cout << "Successfull!";
	getchar();
	getchar();
	return 0;
    }
