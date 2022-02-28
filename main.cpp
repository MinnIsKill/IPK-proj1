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

using namespace std;


/**
 * @brief 
 * https://stackoverflow.com/a/236803
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
 * @brief Get the proc load object
 * 
 * @return current processor load in percentages
 */
unsigned get_proc_load(){
    string snapshot1;
    string snapshot2;

    std::ifstream stats1("/proc/stat"); // get first snapshot
    std::getline(stats1, snapshot1);    // save it
    std::vector<std::string> snapshot1_split = split(snapshot1, ' '); // split it up

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait for 100ms

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

    //totald = Total - PrevTotal where Total = (Idle + NonIdle) and PrevTotal = (PrevIdle + PrevNonIdle) 
    unsigned long long totald = (Idle + NonIdle) - (PrevIdle + PrevNonIdle);
    unsigned long long idled = Idle - PrevIdle;

    //CPU_Percentage = (totald - idled)/totald *100
    //double CPU_Percentage = (double)(totald - idled)/totald *100;

    return (double)(totald - idled)/totald * 100;
}


int main(){
    // Task #1
    char hostname[1024];
    gethostname(hostname, 1024);
    std::cout << hostname << '\n';

    // Task #2
    system("cat /proc/cpuinfo | grep \"model name\" --max-count=1 | awk -F ':' '{print substr($2,2)}'");

    // Task #3
    std::cout << get_proc_load() << "%\n";

    return 0;
}

/**
     user    nice   system  idle      iowait irq   softirq  steal  guest  guest_nice
cpu  74608   2520   24433   1117073   6176   4054  0        0      0      0


PrevIdle = previdle + previowait
Idle = idle + iowait

PrevNonIdle = prevuser + prevnice + prevsystem + previrq + prevsoftirq + prevsteal
NonIdle = user + nice + system + irq + softirq + steal

PrevTotal = PrevIdle + PrevNonIdle
Total = Idle + NonIdle

# differentiate: actual value minus the previous one
totald = Total - PrevTotal
idled = Idle - PrevIdle

CPU_Percentage = (double)(totald - idled)/totald
**/