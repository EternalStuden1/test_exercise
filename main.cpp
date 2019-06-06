#include <winsock2.h>
#include <windows.h>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>


//данную библиотеку (libwsock32.a) я включил в линкер в своем компиляторе
#pragma comment(lib, "ws2_32.lib")

class ExceptionHandler : public std::exception{
private:
    std::string m_cause;

public:
    ExceptionHandler(std::string cause) : m_cause(cause) {}
    const char* what() const noexcept { return m_cause.c_str(); }
};

struct Link {
    std::string link;
    int l_shift;
};

bool ValidUrl(std::string &url){
    std::string http_cmp = "http://";
    // Адрес должен начинаться с "http://"
    if (strncmp(url.c_str(), http_cmp.c_str(), http_cmp.length()) != 0) {
        std::transform(http_cmp.begin(), http_cmp.end(), http_cmp.begin(), toupper);
        if (strncmp(url.c_str(), http_cmp.c_str(), http_cmp.length()) != 0){
            return false;
        }
    }
    return true;
}

std::string DirExists(std::string &path);

char* OpenURL(std::string &url, int mode = 1) { // если mode = 1, то сообщения об ошибках выводятся в консоль
                                                // если mode = 0, то сообщения об ошибках записываются в результат
    try{

    // Решения без использования динамической памяти с запасом я не нашел
    int size=1024*1024;
    char *buf= new char[size]; // переменная для хранения ответа от сервера
    std::string f_url = url;
    f_url.erase(0, 7); //удаляем из строки http:// (7 первых символов)
    WSADATA lpWSAData;
    SOCKET s;
    char *http_host = strdup(f_url.c_str()); // Имя хоста
    int port_num = 80;             // Номер порта по умолчанию
    char *http_path = NULL;
    char *pch = strchr(http_host, ':');
    // Инициализация Winsock.
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0){
        ExceptionHandler error("WSA initialization error" + std::to_string(WSAGetLastError()));
        throw error;
    }
    // Получим имя хоста, номер порта и путь
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
    if (!(hp = gethostbyname(http_host))){
        ExceptionHandler error("Error: incorrect hostname.");
        throw error;
    }
    // Открываем сокет
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET){
        ExceptionHandler error("Error: enable to open socket. Code: " + std::to_string(WSAGetLastError()));
        throw error;
    }
    // объявим переменную для хранения адреса
    sockaddr_in soc_addr = {0};
    soc_addr.sin_family = AF_INET; //тип адреса (TCP/IP)
    soc_addr.sin_addr.S_un.S_un_b.s_b1 = hp->h_addr[0]; //биты IP
    soc_addr.sin_addr.S_un.S_un_b.s_b2 = hp->h_addr[1];
    soc_addr.sin_addr.S_un.S_un_b.s_b3 = hp->h_addr[2];
    soc_addr.sin_addr.S_un.S_un_b.s_b4 = hp->h_addr[3];
    soc_addr.sin_port = htons(port_num); //преобразования порта впонятную для TCP форму

    // Соединяемся с хостом
    if (connect(s, (sockaddr *)&soc_addr, sizeof(soc_addr)) != 0){
        ExceptionHandler error("Error: enable to connect. Code: " + std::to_string(WSAGetLastError()));
        throw error;
    }

    // Формируем HTTP запрос
    std::string query = "";
    query += "GET /";
    query += http_path;
    query += " HTTP/1.1\r\nHost: ";
    query += http_host;
    query += "\r\nConnection: close\r\n\r\n";

    // Отправляем запрос серверу
    int cnt = send(s, query.c_str(), strlen(query.c_str()), 0);
    // Проверяем, не произошло ли ошибки при отправке запроса на сервер
    if (cnt == SOCKET_ERROR){
        ExceptionHandler error("Error sending request to the server. Error code: " + std::to_string(WSAGetLastError()));
        throw error;
    }

    // Получаем ответ с сервера.
    char *result_ptr = buf;
    cnt = 1;
    while (cnt > 0 && size > 0){
        cnt = recv(s, result_ptr, sizeof(size), 0);
        if (cnt > 0){
            result_ptr += cnt;
            size -= cnt;
        }
    }

    // Деинициализация библиотеки Ws2_32.dll
    WSACleanup();

    return(buf);
    }

    catch(ExceptionHandler coughtError){
        WSACleanup();
        if (mode == 1){
            std::cout << coughtError.what() << std::endl;
            return NULL;
        }
        else{
            return const_cast<char *>(coughtError.what());
        }



     }
    catch(std::exception coughtError){
        if (mode == 1)
            std::cout << coughtError.what() << std::endl;
        WSACleanup();
        return NULL;
    }
}

//функция для поиска в html-коде ссылок
// У функции странный интерфейс. Почему Link не возвращается? - Link не возвращается, так как функция записывает результат в структуру-параметр
void FindLink(std::string content, std::string descriptor, Link &CurrentLink) {
    int hrefShift = 6; // сдвиг на 6 позиций - для перехода от href="url" к url"
    CurrentLink.l_shift = content.find(descriptor, CurrentLink.l_shift);
    if (CurrentLink.l_shift != -1) {
        CurrentLink.l_shift = content.find("href=", CurrentLink.l_shift);
        CurrentLink.l_shift += hrefShift;
        CurrentLink.link.assign(content, CurrentLink.l_shift, content.find('"', CurrentLink.l_shift) - CurrentLink.l_shift);
    }
}

//функция для записи ссылок в файл Link_x.txt по указанному пути
void SaveLinks(std::string content, std::string path) {
    std::ofstream fout;
    int counter = 1;
    Link CurrentLink;
    CurrentLink.l_shift = 0;
    char *result;
    while (CurrentLink.l_shift != -1) {
        FindLink(content, "<a", CurrentLink);
        fout.open(path + "Link_" + std::to_string(counter) + ".txt");
        fout << "Link: " << CurrentLink.link << "\nContent:\n";
        if (!ValidUrl(CurrentLink.link)){
            fout << "Invalid link!";
            fout.close();
        }
        else{
            result = OpenURL(CurrentLink.link, 0);
            fout << result;
            fout.close();
            counter++;
        }
    }
    CurrentLink.l_shift = 0;
    while (CurrentLink.l_shift != -1) {
        FindLink(content, "<link", CurrentLink);
        fout << "Link: " << CurrentLink.link << "\nContent:\n";
        fout << "Link: " << CurrentLink.link << "\nContent:\n";
        if (!ValidUrl(CurrentLink.link)){
            fout << "Invalid link!";
            fout.close();
        }
        else{
            result = OpenURL(CurrentLink.link, 0);
            fout.open(path + "Link_" + std::to_string(counter) + ".txt");
            fout << result;
            fout.close();
            counter++;
        }
    }
    delete result;
}

std::string DirExists(std::string &path)
{
    DWORD code = GetFileAttributes(path.c_str());
    if (code == INVALID_FILE_ATTRIBUTES)
        return "\nDoesntExists. Cause code: " + std::to_string(GetLastError());
    else{
        std::ofstream fout(path + "/Base.txt"); //проверка на возможность создания файлов в директории для текущего пользователя
        if (!fout) return "Not enough rights to create new file here.";
        return (FILE_ATTRIBUTE_DIRECTORY & code) != 0 ? "exists" : "\nIt`s file, not directory";
    }
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "rus");
    if (argc != 3){
        std::cout << "Incorrect arguments!" << std::endl;
        return 0;
    }
    std::string url = argv[1];
    std::string path = argv[2];
    if (!ValidUrl(url)){
        std::cout << "Invalid URL!";
    }
    else{
        if (DirExists(path) != "exists") {
            std::cout << DirExists(path) << std::endl;
            return 0;
        }
        char *result = OpenURL(url, 1);
        if (result) {
            std::ofstream fout;
            fout.open(std::string(path) + "/" + "Base.txt");
            fout << result;
            fout.close();
            path += "/";
            SaveLinks(result, path);
        }
        delete result;
        result = 0;
        std::cout << "Successfull!";
    }
    getchar();
    return 0;
}
