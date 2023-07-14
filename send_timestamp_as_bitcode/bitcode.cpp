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
#include <chrono>
#include <iostream>
#include <thread>

constexpr int BITCODE_LENGTH = 68;                     // 64 bits for timestamp + 4 bits for start/end of bitcode
constexpr float64 SAMPLE_RATE = 1000.0;                // Hz
constexpr int READ_ARRAY_LENGTH = BITCODE_LENGTH + 1;  // Read 1 sample more than write
std::atomic<uint64_t> tsInAtomic(0);                   // Thread safe timestamp
std::atomic<bool> keepSendingBitcodeFlag(true);        // For checking if timestamp has changed

/**
 * @brief Handles error from NI-DAQmx functions.
 *
 * @param err NI-DAQmx error code.
 */
inline void handleError(int err) {
    if (err == 0)
        return;

    char error[1024];
    DAQmxGetErrorString(err, error, 1024);
    std::cout << error << std::endl;
}

/**
 * @brief Gets the current CPU clock time in microseconds.
 *
 * @return uint64_t microseconds since last boot.
 */
uint64_t getCPUClockTimeUS() {
    std::chrono::duration<double> time = std::chrono::steady_clock::now().time_since_epoch();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);

    uint64_t usCount = us.count();
    return usCount;
}

/**
 * @brief Converts an integer to a binary string.
 *
 * @param n integer to convert
 * @return std::string
 */
std::string convertIntToBinary(uint64_t n) {
    std::string result;
    while (n != 0) {
        result = (n % 2 == 0 ? "0" : "1") + result;
        n /= 2;
    }
    return result;
}

/**
 * @brief Converts an integer to a bitcode array.
 *
 * The bitcode array has length BITCODE_LENGTH. The first two bits are "01" and the last two bits are "10", which
 * signify the start/end of a bitcode signal. The middle bits are the binary representation of the integer, padded with
 * leading zeros.
 *
 * @param n integer to convert
 * @param bitcodeLength length of bitcode array
 * @param writeArray array to write bitcode to
 */
void convertIntToBitcode(uint64_t n, int bitcodeLength, uInt8* writeArray) {
    // Convert n to binary string
    std::string binary = convertIntToBinary(n);

    // Pad with leading zeros
    while (binary.length() < bitcodeLength - 4) {
        binary = "0" + binary;
    }

    // Insert into bitcode,  Set first/last bit to 0 and second/second-to-last bit to 1
    std::string bitcode = "01" + binary + "10";

    // Convert to uInt8 array
    for (int i = 0; i < bitcode.length(); i++) {
        if (bitcode[i] == '0') {
            writeArray[i] = 0;
        } else {
            writeArray[i] = 1;
        }
    }
}

/**
 * @brief Converts a bitcode array to an integer.
 *
 * @param readArray array of bits read from read_hw task. Should have length BITCODE_LENGTH+1.
 * @return uint64_t
 */
uint64_t convertReadArrayToInt(uInt8* readArray) {
    // Convert array to binary string, ignoring first/last bits that are always HIGH. Note that in the bitcode, these
    // refer to the second/second-to-last bit bits.
    std::string binary;
    for (int i = 1; i < READ_ARRAY_LENGTH; i++) {
        if (readArray[i] == 0) {
            binary += "0";
        } else {
            binary += "1";
        }
    }

    // Convert binary string to int
    // Because binary string starts with "01" and ends with "10", which are the first/last two bits of the bitcode that
    // signify the start/end of the bitcode, we want to ignore those when converting to the timestamp, so index from 2 and
    // go to length-2.
    uint64_t n = 0;
    for (int i = 2; i < binary.length() - 2; i++) {
        n = n * 2 + binary[i] - '0';
    }

    return n;
}

/**
 * @brief Sends a timestamp as a bitcode pulse using a NI-DAQ board.
 *
 * This function first sends a software-triggered HIGH signal, which is used as the timing signal on the Intan board. It
 * then sends a sequence of hardware-timed signals corresponding to the bitcode, which conveys the timestamp.
 *
 * @param writeHw handle to a hardware write task
 * @param readHw handle to a hardware read task
 * @param writeSw handle to a software write task
 * @param readSw  handle to a software read task
 * @return uint64_t
 */
uint64_t sendTimestampAsBitcodePulse(TaskHandle& writeHw, TaskHandle& readHw, TaskHandle& writeSw, TaskHandle& readSw) {
    /////////////////
    /*Software HIGH*/
    /////////////////

    // A single software write has much lower latency than a hardware-timed pulse, so we use this software HIGH as
    // the timing signal on the intan board.

    uInt8 swWrite1[1] = {1};
    handleError(DAQmxWriteDigitalLines(writeSw, 1, true, 1, DAQmx_Val_GroupByChannel, swWrite1, NULL, NULL));
    uint64_t swTime = getCPUClockTimeUS();
    std::cout << "swT: " << swTime - tsInAtomic << "us" << std::endl;

    ////////////////////////////////
    /*Hardware timed bitcode pulse*/
    ////////////////////////////////

    // This hardware-timed bitcode conveys the timestamp, thus acting as a label for linking the Intan data to the Robot
    // PC state data.

    // Convert timestamp to bitcode
    uInt8 writeArray[BITCODE_LENGTH];
    convertIntToBitcode(tsInAtomic, BITCODE_LENGTH, writeArray);

    // Write bitcode; does not write until triggered by start of read task
    handleError(DAQmxWriteDigitalLines(writeHw, BITCODE_LENGTH, true, 1, DAQmx_Val_GroupByChannel, writeArray, 0, NULL));

    // Read written bitcode; this triggers the write task. The read task data trails the write task by 1 sample.
    uInt8 readArray[READ_ARRAY_LENGTH];
    handleError(DAQmxReadDigitalLines(readHw, READ_ARRAY_LENGTH, 1, DAQmx_Val_GroupByChannel, readArray,
                                      sizeof(readArray), NULL, NULL, NULL));

    // Stop hardware tasks - necessary to be retriggerable
    handleError(DAQmxStopTask(writeHw));
    handleError(DAQmxStopTask(readHw));

    ////////////////
    /*Software LOW*/
    ////////////////

    // Write LOW for timing signal; indicates the end of the timing signal
    swWrite1[0] = {0};
    handleError(DAQmxWriteDigitalLines(writeSw, 1, true, 1, DAQmx_Val_GroupByChannel, swWrite1, NULL, NULL));

    /////////////////////////////////////////////
    /*Compare timestamp sent and timestamp read*/
    /////////////////////////////////////////////

    // Convert back to timestamp
    uint64_t tsOut = convertReadArrayToInt(readArray);
    // Compare tsIn and tsOut
    if (tsInAtomic != tsOut) {
        std::cout << "Failure for timestamp: " << tsInAtomic << std::endl;
    }

    return tsOut;
}

/**
 * @brief Initializes NIDAQ tasks and sends bitcode pulses as new timestamps are received.
 *
 * This function operates as a separate thread. When it detects a changes in the atomic variable tsInAtomic, it sends
 * the bitcode.
 *
 */
void bitcodeSender() {
    uint64_t ts;
    ////////////////////////
    /* Initialize Channels*/
    ////////////////////////

    // Create hardware read task and DI channel
    TaskHandle readHw;
    handleError(DAQmxCreateTask("readHw", &readHw));
    handleError(DAQmxCreateDIChan(readHw, "Dev2/port0/line0", "channel0", DAQmx_Val_ChanForAllLines));
    handleError(
        DAQmxCfgSampClkTiming(readHw, "", SAMPLE_RATE, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, READ_ARRAY_LENGTH));

    // Create hardware write task and DO channel; trigger with readHw start
    TaskHandle writeHw;
    handleError(DAQmxCreateTask("writeHw", &writeHw));
    handleError(DAQmxCreateDOChan(writeHw, "Dev2/port0/line1", "channel1", DAQmx_Val_ChanForAllLines));
    handleError(DAQmxCfgSampClkTiming(writeHw, "", SAMPLE_RATE, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, BITCODE_LENGTH));
    handleError(DAQmxCfgDigEdgeStartTrig(writeHw, "/Dev2/di/StartTrigger", DAQmx_Val_Rising));

    // Create software read task and DI channel
    TaskHandle readSw;
    handleError(DAQmxCreateTask("readSw", &readSw));
    handleError(DAQmxCreateDIChan(readSw, "Dev2/port0/line2", "channel2", DAQmx_Val_ChanForAllLines));

    // Create software write task and DO channel
    TaskHandle writeSw;
    handleError(DAQmxCreateTask("writeSw", &writeSw));
    handleError(DAQmxCreateDOChan(writeSw, "Dev2/port0/line3", "channel3", DAQmx_Val_ChanForAllLines));

    // Initial software read/write; makes subsequent sw read/writes much faster
    uInt8 swRead1[1] = {0};
    uInt8 swWrite1[1] = {0};

    // write LOW;read one sample
    handleError(DAQmxWriteDigitalLines(writeSw, 1, true, 1, DAQmx_Val_GroupByChannel, swWrite1, NULL, NULL));
    handleError(
        DAQmxReadDigitalLines(readSw, 1, 1, DAQmx_Val_GroupByChannel, swRead1, sizeof(swRead1), NULL, NULL, NULL));

    ////////////////////////////////
    /*Transmit Timestamp as Pulses*/
    ////////////////////////////////

    // tsPrev is used to check if the timestamp has changed
    uint64_t tsPrev = tsInAtomic;
    uint64_t tsOut;

    // Loop until timestamp changes
    while (keepSendingBitcodeFlag) {
        // If timestamp has changed, send bitcode pulse
        if (tsInAtomic != tsPrev) {
            tsOut = sendTimestampAsBitcodePulse(writeHw, readHw, writeSw, readSw);

            uint64_t tsFinal = getCPUClockTimeUS();

            std::cout << "RTT: " << tsFinal - tsInAtomic << "us" << std::endl;

            // Print variables
            std::cout << "**********************" << std::endl;
            tsPrev = tsInAtomic;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));  // Allow time on other threads
    }
}
