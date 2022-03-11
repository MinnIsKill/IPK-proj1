#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <iterator>
#include <unistd.h>
#include <stdlib.h>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;
int ServerSocket, NewSocket;

/** ./hinfosvc 12345 &
 *  curl http://localhost:12345/hostname
 *  curl http://localhost:12345/cpu-name
 *  curl http://localhost:12345/load
**/ 

/**
 * @brief splits a string into a vector of strings based on delim
 * taken and modified from: https://stackoverflow.com/a/236803
 * 
 * @param s: string to be split
 * @param delim: delimiter based on which to split string
 */
template <typename Out>
void split(const string &s, char delim, Out result){
    istringstream iss(s);
    string item;
    while (getline(iss, item, delim)){
        *result++ = item;
    }
}
vector<string> split(const string &s, char delim){
    vector<string> elems;
    split(s, delim, back_inserter(elems));
    return elems;
}

/**
 * @brief Get the current processor load
 * 
 * @return current processor load in percentages
 */
unsigned get_proc_load(){
    string snapshot1;
    string snapshot2;

    ifstream stats1("/proc/stat"); // get first snapshot
    getline(stats1, snapshot1);    // save it
    vector<string> snapshot1_split = split(snapshot1, ' '); // split it up

    this_thread::sleep_for(chrono::milliseconds(300)); // wait for 300ms

    ifstream stats2("/proc/stat"); // get second snapshot
    getline(stats2, snapshot2);    // save it
    vector<string> snapshot2_split = split(snapshot2, ' '); // split it up

    /** DO THE MATH **/
    // PrevIdle = previdle + previowait
    unsigned long long PrevIdle = stoi(snapshot1_split[5]) + stoi(snapshot1_split[6]);
    // Idle = idle + iowait
    unsigned long long Idle = stoi(snapshot2_split[5]) + stoi(snapshot2_split[6]);
    // PrevNonIdle = prevuser + prevnice + prevsystem + previrq + prevsoftirq + prevsteal
    unsigned long long PrevNonIdle = stoi(snapshot1_split[2]) + stoi(snapshot1_split[3]) + stoi(snapshot1_split[4]) 
                                   + stoi(snapshot1_split[7]) + stoi(snapshot1_split[8]) + stoi(snapshot1_split[9]);
    // NonIdle = user + nice + system + irq + softirq + steal
    unsigned long long NonIdle = stoi(snapshot2_split[2]) + stoi(snapshot2_split[3]) + stoi(snapshot2_split[4]) 
                               + stoi(snapshot2_split[7]) + stoi(snapshot2_split[8]) + stoi(snapshot2_split[9]);

    // totald = Total - PrevTotal where Total = (Idle + NonIdle) and PrevTotal = (PrevIdle + PrevNonIdle) 
    unsigned long long totald = (Idle + NonIdle) - (PrevIdle + PrevNonIdle);
    unsigned long long idled = Idle - PrevIdle;

    return (double)(totald - idled)/totald * 100;
}

/**
 * @brief works the same way as system() command but saves the output string
 * 
 * @param command: regex to execute
 * @return string returned by regex
 */
string my_system_func(const char* command) {
    char buff[256];
    string result = "";
    FILE* fp = popen(command, "r");

    while (fgets(buff, sizeof(buff), fp) != NULL) {
        result += buff;
    }

    pclose(fp);
    return result;
}

/**
 * @brief handles server closing when turned off using CTRL+C
 */
void signalHandler(int signum){
    shutdown(NewSocket, SHUT_RDWR);
    close(NewSocket);
    shutdown(ServerSocket, SHUT_RDWR);
    close(ServerSocket);
    _exit (signum);
}



/*****************************************
 * @brief MAIN FUNCTION
 * 
 * @param argc: arguments count
 * @param argv: arguments pointer
 * @return 1 if error found, 0 otherwise
*****************************************/
int main(int argc, char *argv[]){
    int opt = 1;
    int PORT;
    int valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024];
    char hello[256] = "Hello from Server";

    if (argc == 2){
        if (isdigit(*argv[1])){
            PORT = atoi(argv[1]);
            //printf("argv[1]:  %d \n",PORT);
            //printf("PORT:  %d \n",PORT);
        }
    } else {
        fprintf(stderr,"argc error \n");
        exit(EXIT_FAILURE);
    }

    if ((ServerSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // create socket
        fprintf(stderr,"socket error \n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        fprintf(stderr,"setsockopt error \n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))){
        fprintf(stderr,"setsockopt error \n");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(ServerSocket, (struct sockaddr*)&address, sizeof(address)) < 0){
        fprintf(stderr,"bind error \n");
        exit(EXIT_FAILURE);
    }

    if (listen(ServerSocket, 3) < 0){
        fprintf(stderr,"listen error \n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT,signalHandler);

    // main loop
    while ((NewSocket = accept(ServerSocket, (struct sockaddr*)&address, (socklen_t*)&addrlen)) >= 0) {
        valread = read(NewSocket, buffer, 1024);
        // Task #1
        if (strncmp(buffer,"GET /hostname HTTP/1.1",22) == 0){
            char hostname[256];
            gethostname(hostname, 256);

            string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += hostname;
            msg += "\r\n";

            const char *message = msg.c_str();
            send(NewSocket,message,strlen(message),0);

            close(NewSocket);
        // Task #2
        } else if (strncmp(buffer,"GET /cpu-name HTTP/1.1",22) == 0){
            string cpu_name = my_system_func("cat /proc/cpuinfo | grep \"model name\" --max-count=1 | awk -F ':' '{print substr($2,2)}'");

            string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += cpu_name;

            const char *message = msg.c_str();
            send(NewSocket,message,strlen(message),0);

            close(NewSocket);
        // Task #3
        } else if (strncmp(buffer,"GET /load HTTP/1.1",18) == 0){
            unsigned load = get_proc_load();

            string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += to_string(load);
            msg += "%\r\n";

            const char *message = msg.c_str();
            send(NewSocket,message,strlen(message),0);

            close(NewSocket);
        // Error
        } else {
            string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += "400 Bad Request\r\n";

            const char *message = msg.c_str();
            send(NewSocket,message,strlen(message),0);

            close(NewSocket);
        }
    }

    return 0;
}