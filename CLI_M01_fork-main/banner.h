#ifndef BANNER_H
#define BANNER_H

#include <iostream>

// Initial banner when booting up
inline void showBanner() {
    // ASCII Text
    std::cout << R"(  
 _____  _____  ___________ _____ _______   __
/  __ \/  ___||  _  | ___ \  ___/  ___\ \ / /
| /  \/\ `--. | | | | |_/ / |__ \ `--. \ V / 
| |     `--. \| | | |  __/|  __| `--. \ \ /  
| \__/\/\__/ /\ \_/ / |   | |___/\__/ / | |  
 \____/\____/  \___/\_|   \____/\____/  \_/ 
    )" << '\n';
    std::cout <<"--------------------------------------------\n"; 
    std::cout << "Hello, Welcome to CSOPESY commandline!\n";
    std::cout <<"Developers:\n";
    std::cout <<"Ambata, Simon Luis \nAmores, Louise Carlo \nOno, Shintaro \nPaule, Mikael Angelo\n";
    std::cout <<"--------------------------------------------\n"; 
}

#endif