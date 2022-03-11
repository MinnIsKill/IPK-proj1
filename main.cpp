/**
 * @file main.cpp
 * @author Vojtech Kalis (xkalis03@stud.fit.vutbr.cz)
 * @date 2022-03-11
 * 
 * @brief: main file of a HTTP server script for IPK project #1 VUTBR 2021/22
**/

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

int ServerSocket, NewSocket;

/**
 * @brief splits a string into a vector of strings based on delim
 * taken and modified from: https://stackoverflow.com/a/236803
 * 
 * @param s: string to be split
 * @param delim: delimiter based on which to split string
 */
template <typename Out>
void split(const std::string &s, char delim, Out result){
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim)){
        *result++ = item;
    }
}
std::vector<std::string> split(const std::string &s, char delim){
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

/**
 * @brief Get the current processor load
 * 
 * @return current processor load in percentages
 */
unsigned get_proc_load(){
    std::string snapshot1;
    std::string snapshot2;

    std::ifstream stats1("/proc/stat"); // get first snapshot
    std::getline(stats1, snapshot1);    // save it
    std::vector<std::string> snapshot1_split = split(snapshot1, ' '); // split it up

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // wait for 500ms

    std::ifstream stats2("/proc/stat"); // get second snapshot
    std::getline(stats2, snapshot2);    // save it
    std::vector<std::string> snapshot2_split = split(snapshot2, ' '); // split it up

    /** DO THE MATH **/
    // PrevIdle = previdle + previowait
    unsigned long long PrevIdle = std::stoi(snapshot1_split[5]) + std::stoi(snapshot1_split[6]);
    // Idle = idle + iowait
    unsigned long long Idle = std::stoi(snapshot2_split[5]) + std::stoi(snapshot2_split[6]);
    // PrevNonIdle = prevuser + prevnice + prevsystem + previrq + prevsoftirq + prevsteal
    unsigned long long PrevNonIdle = std::stoi(snapshot1_split[2]) + std::stoi(snapshot1_split[3]) + std::stoi(snapshot1_split[4]) 
                                   + std::stoi(snapshot1_split[7]) + std::stoi(snapshot1_split[8]) + std::stoi(snapshot1_split[9]);
    // NonIdle = user + nice + system + irq + softirq + steal
    unsigned long long NonIdle = std::stoi(snapshot2_split[2]) + std::stoi(snapshot2_split[3]) + std::stoi(snapshot2_split[4]) 
                               + std::stoi(snapshot2_split[7]) + std::stoi(snapshot2_split[8]) + std::stoi(snapshot2_split[9]);

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
std::string my_system_func(const char* command) {
    char buff[256];
    std::string result = "";
    std::FILE* fp = popen(command, "r");

    while (std::fgets(buff, sizeof(buff), fp) != NULL) {
        result += buff;
    }

    pclose(fp);
    return result;
}

/**
 * @brief Very roughly checks if received header format is in a somewhat acceptable state
 * 
 * @param header string carrying the header
 * @param init_cut length of first header line, which has already been check and thus can be skipped
 * @return true if header's okay, false if error found
 */
bool is_header_okay (std::string header, int init_cut){
    //const char *message = header.c_str();
    //std::fprintf(stdout,"%s\n",message);
    header.erase(0,init_cut+1); //remove first line

    if ((header.compare(0,6,"\nHost:") == 0) || (header.compare(0,6,"\rHost:") == 0)){ //"Host:" found
        header.erase(0,6);
        while((header.compare(0,1,"\n") != 0) && (header.compare(0,1,"\r") != 0)){ //find eol
            header.erase(0,1);
        }
        header.erase(0,1);
        if ((header.compare(0,12,"\nUser-Agent:") == 0) || (header.compare(0,12,"\rUser-Agent:") == 0)){ //"User-Agent:" found
            header.erase(0,12);
            while((header.compare(0,1,"\n") != 0) && (header.compare(0,1,"\r") != 0)){ //find eol
                header.erase(0,1);
            }
            header.erase(0,1);
            if ((header.compare(0,8,"\nAccept:") == 0) || (header.compare(0,8,"\rAccept:") == 0)){ //"Accept:" found
                header.erase(0,8);
                while((header.compare(0,1,"\n") != 0) && (header.compare(0,1,"\r") != 0)){ //find eol
                    header.erase(0,1);
                }
            } else { return false; } //error
        } else { return false; } //error
    } else { return false; } //error

    return true;
}

/**
 * @brief Handles server closing when turned off using CTRL+C
 */
void signalHandler(int signum){
    shutdown(NewSocket, SHUT_RDWR);
    close(NewSocket);
    shutdown(ServerSocket, SHUT_RDWR);
    close(ServerSocket);
    _exit (signum);
}

/**
 * @brief Handles server closing when error occurs
 */
void my_exit(){
    shutdown(NewSocket, SHUT_RDWR);
    close(NewSocket);
    shutdown(ServerSocket, SHUT_RDWR);
    close(ServerSocket);
    std::exit(EXIT_FAILURE);
}

/**
 * @brief Handles accept socket closing when error with passed header found
 */
void header_stop(){
    std::fprintf(stderr,"ERROR: Found a problem with received header.\r\n");
    //const char *message = "ERROR: A problem with your passed header was found.\r\n";
    //send(NewSocket,message,std::strlen(message),0);
    shutdown(NewSocket, SHUT_RDWR);
    close(NewSocket);
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

    if (argc == 2){
        if ((std::isdigit(*argv[1])) && ((std::strlen(argv[1]) == 4) || (std::strlen(argv[1]) == 5))){
            PORT = atoi(argv[1]);
            //printf("argv[1]:  %d \n",PORT);
            //printf("PORT:  %d \n",PORT);
        } else {
            std::fprintf(stderr,"ERROR: Second argument either NaN or the given number does not conform to required length limit of 4 to 5 digits.\r\n");
            my_exit();
        }
    } else {
        std::fprintf(stderr,"ERROR: Too many/few arguments received.\r\n");
        my_exit();
    }

    if ((ServerSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // create socket
        std::fprintf(stderr,"ERROR: Socket creation error.\r\n");
        my_exit();
    }
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        std::fprintf(stderr,"ERROR: Socket options modifying error.\r\n");
        my_exit();
    }
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))){
        std::fprintf(stderr,"ERROR: Socket options modifying error.\r\n");
        my_exit();
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(ServerSocket, (struct sockaddr*)&address, sizeof(address)) < 0){
        std::fprintf(stderr,"ERROR: Socket binding error.\r\n");
        my_exit();
    }

    if (listen(ServerSocket, 3) < 0){
        std::fprintf(stderr,"ERROR: Socket listen error.\r\n");
        my_exit();
    }

    signal(SIGINT,signalHandler);

    // main loop
    while ((NewSocket = accept(ServerSocket, (struct sockaddr*)&address, (socklen_t*)&addrlen)) >= 0) {
        valread = read(NewSocket, buffer, 1024);
        // Task #1
        if (std::strncmp(buffer,"GET /hostname HTTP/1.1",22) == 0){
            std::string header = buffer;
            if (is_header_okay(header,22) == false){
                header_stop();
            }
            char hostname[256];
            gethostname(hostname, 256);

            std::string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += hostname;
            msg += "\r\n";

            const char *message = msg.c_str();
            send(NewSocket,message,std::strlen(message),0);

            close(NewSocket);
        // Task #2
        } else if (std::strncmp(buffer,"GET /cpu-name HTTP/1.1",22) == 0){
            std::string header = buffer;
            if (is_header_okay(header,22) == false){
                header_stop();
            }
            std::string cpu_name = my_system_func("cat /proc/cpuinfo | grep \"model name\" --max-count=1 | awk -F ':' '{print substr($2,2)}'");

            std::string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += cpu_name;

            const char *message = msg.c_str();
            send(NewSocket,message,std::strlen(message),0);

            close(NewSocket);
        // Task #3
        } else if (std::strncmp(buffer,"GET /load HTTP/1.1",18) == 0){
            std::string header = buffer;
            if (is_header_okay(header,18) == false){
                header_stop();
            }
            unsigned load = get_proc_load();

            std::string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += std::to_string(load);
            msg += "%\r\n";

            const char *message = msg.c_str();
            send(NewSocket,message,std::strlen(message),0);

            close(NewSocket);
        // Error
        } else {
            std::string msg ("HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n");
            msg += "400 Bad Request\r\n";

            const char *message = msg.c_str();
            send(NewSocket,message,std::strlen(message),0);

            close(NewSocket);
        }
    }

    return 0;
}