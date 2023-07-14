#include <NIDAQmx.h>

#include <chrono>
#include <iostream>

const float64 kCameraFps = 100.0;      // Hz
const float64 kPulseDutyCycle = 0.25;  // 25% duty cycle

/**
 * @brief Handles error from NI-DAQmx functions.
 *
 * @param err NI-DAQmx error code.
 */
inline void HandleError(int error) {
    if (error == 0)
        return;

    char errorMessage[1024];
    DAQmxGetErrorString(error, errorMessage, 1024);
    std::cout << errorMessage << std::endl;
}

int main() {
    // Create counter output task; ctr0 corresponds to terminal PFI12
    TaskHandle counterTaskHandle;
    HandleError(DAQmxCreateTask("counter_task", &counterTaskHandle));
    HandleError(DAQmxCreateCOPulseChanFreq(counterTaskHandle, "Dev2/ctr0", "counter", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0,
                                           kCameraFps, kPulseDutyCycle));
    DAQmxCfgImplicitTiming(counterTaskHandle, DAQmx_Val_ContSamps, 1000);

    // Start counter output task
    HandleError(DAQmxStartTask(counterTaskHandle));

    // Wait for user input; pulses continue until user presses enter
    std::getchar();

    // Stop task
    HandleError(DAQmxStopTask(counterTaskHandle));

    return 0;
}
