# Code for prototyping a NIDAQMX counter that sends hardware trigger pulses to the cameras

import time

start = time.time()
import nidaqmx
from nidaqmx.constants import AcquisitionType
from nidaqmx.types import CtrTime

CAMERA_FPS = 100  # hz

with nidaqmx.Task() as co_task, nidaqmx.Task() as ci_task:
    # # ctr1-src defaults to line PFI3
    # ci = ci_task.ci_channels.add_ci_count_edges_chan("Dev2/ctr1")
    # ci_task.start()

    # ctr0-Out defaults line PFI12 (see manual https://docs-be.ni.com/bundle/pcie-pxie-usb-63xx-features/raw/resource/enus/370784k.pdf)
    co = co_task.co_channels.add_co_pulse_chan_freq(
        "Dev2/ctr0",
        freq=CAMERA_FPS,
        duty_cycle=0.2,
    )
    co_task.timing.cfg_implicit_timing(sample_mode=AcquisitionType.CONTINUOUS)

    start = time.time()
    co_task.start()

    input()
    stop = time.time()

    # print("Expected number of pulses: ", (stop - start) * CAMERA_FPS)
    # print("Actual number of pulses: ", ci.ci_count)

    # ci_task.timing.cfg_implicit_timing(sample_mode=AcquisitionType.CONTINUOUS)
