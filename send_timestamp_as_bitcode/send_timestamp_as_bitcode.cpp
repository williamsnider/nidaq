#include <NIDAQmx.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "bitcode.cpp"

int main() {
    // Create bitcode thread
    std::thread bitcodeThread(bitcodeSender);

    // Sleep to allow thread to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (int i = 0; i < 25; i++) {
        // Get timestamp
        tsInAtomic = getCPUClockTimeUS();  // atomic variable

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    keepSendingBitcodeFlag = false;

    bitcodeThread.join();

    return 0;
}