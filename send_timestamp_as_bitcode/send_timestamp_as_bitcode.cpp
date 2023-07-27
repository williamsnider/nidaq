/**
 * This file demonstrates the use of a NIDAQ board to send a hardware-timed bitcode that corresponds to a timestamp.
 *
 * The main thread obtains new timestamps, and a separate thread controls the NIDAQ board and sends the bitcode pulses.
 *
 * In our setup, we are using a NI PCIe-6321 board.
 * Channels Dev2/port0/line0 and Dev2/port0/line1 are physically connected.
 * Channels Dev2/port0/line2 and Dev2/port0/line3 are physically connected.
 */

#include <NIDAQmx.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "bitcode.cpp"

int main()
{

    std::atomic<bool> keepSendingBitcodeFlag(true);
    std::atomic<bool> *keepSendingBitcodeFlag_ptr = &keepSendingBitcodeFlag;

    // Create bitcode thread
    std::thread bitcodeThread(bitcodeSender, keepSendingBitcodeFlag_ptr);

    // Sleep to allow thread to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (int i = 0; i < 1; i++)
    {
        // Get timestamp
        tsInAtomic = getCPUClockTimeUS(); // atomic variable
        std::cout << "Timestamp: " << tsInAtomic << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    *keepSendingBitcodeFlag_ptr = false;
    bitcodeThread.join();

    return 0;
}