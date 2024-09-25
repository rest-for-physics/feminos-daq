#!/usr/bin/env python3

import subprocess
import sys
import uproot
import os
import awkward as ak

config_contents = """
fem * #Apply first to both feminos
##################################
# 1 Feminos + 1 FEC AGET
# Test of self trigger
##################################
#
##################################
# Result file path
##################################
path /home/useriaxo/data
file_chunk 1024	#(MiB) Added by Hector 5Jun20
# file_chunk 100 # MB
##################################
# Event Builder settings
##################################
eof_on_eoe 1	#0 en Canfranc
event_builder 0x1	#0 en Canfranc
##################################
# Ring Buffer and DAQ cleanup
##################################
DAQ 0
credits restore 12 1 F	#8 en Canfranc
daq 0xFFFFFF F
sca enable 0
sleep 1
serve_target 0
sleep 4
cmd clr #No está en Canfranc
##################################
# Feminos Settings
##################################
# sca cnt 0x200
sca cnt 0x200
#sca autostart 1
#rst_len 1
##################################
# Readout settings
##################################
modify_hit_reg 0	# We dont know what this does here, we want it to be 1 to allow for forceon/off changes. It is changed later
# Allow erase of hit register
erase_hit_ena 0
# Allowable hit limit threshold
erase_hit_thr * 2
emit_hit_cnt 1
emit_empty_ch 0
emit_lst_cell_rd 0
keep_rst 0
skip_rst 1
##################################
# AGET settings
##################################
# Mode: 0x0: hit/selected channels 0x1:all channels
aget * mode 0x1		# = 1 + forceon 1 -> save all channels // = 0 + forceon 0 -> save only hits
aget * tst_digout 0
# Common coarse part of discriminator threshold
fem 0
aget 0 dac 0x2
aget 1 dac 0x2
aget 2 dac 0x2
aget 3 dac 0x2
# Individual channel fine part of discriminator threshold
aget 0 threshold * 0xF
aget 1 threshold * 0xF
aget 2 threshold * 0xF
aget 3 threshold * 0xF
#aget 0 threshold 4:64 0xf
fem *
##################################
# Channel ena/disable (AGET only)
##################################
modify_hit_reg 1	# It must be 1 for forceon/off changes to have effect. DON'T CHANGE!!
forceon_all 0		# If 1 it will dominate over forceon and all channels will be saved. DON'T CHANGE!!
forceon * * 1		# = 0 save only hits channels, = 1 save all channels
forceoff * * 0		# = 1 don't save any channel even if the threshold is overcome. DON'T CHANGE!!

##################################
# Pedestal Thresholds and Zero-suppression
# (keep current settings)
##################################
subtract_ped 1
zero_suppress 0
zs_pre_post 8 4
##################################
# Multiplicity Trigger settings
##################################
sleep 1
fem 0
mult_thr 0 32
mult_thr 1 32
mult_thr 2 32
mult_thr 3 32
mult_limit 0 230
mult_limit 1 230
mult_limit 2 230
mult_limit 3 230
fem *
mult_trig_ena 1
#snd_mult_ena 0
##################################
# Event generator
##################################
# Event limit: 0x0:infinite; 0x1:1; 0x2:10; 0x3:100; 0x4: 1000; 0x5:10000; 0x6:100000; 0x7:1000000
event_limit 0x0
fem 0
trig_delay 0x3E8	1000
#trig_delay 0x190	400
#trig_delay 0x1C2	450
#trig_delay 0x1F4	500
#trig_delay 0xfa0	4000

# Range: 0:0.1Hz-10Hz 1:10Hz-1kHz 2:100Hz-10kHz 3:1kHz-100kHz
#trig_rate 3 1
fem *
# Trigger enable bits (OR several if desired): 0x1:generator 0x2:trigger pin 0x4:pulser 0x8: TCM
trig_enable 0x0
##################################
# Set Data server target:
#  0: drop data
#  1: send to DAQ
#  2: feed to pedestal histos
#  3: feed to hit channel histos
##################################
serve_target 1
##################################
# Clear dead-time histogram
##################################
fem 0
busy_resol 3
fem *
hbusy clr
hhit clr *
##################################
# Create result file
##################################
fopen
##################################
# Enable data taking
##################################
sca enable 1
##################################
# Data acquisition
##################################
DAQ 20000000000000
# DAQ 5600000000
##################################
# Wait DAQ completion
##################################

LOOP 30

DAQ
sleep 1
NEXT
##################################
# Show dead-time histogram
##################################
hbusy get
##################################
# Close result file
##################################
fclose
"""

# the config file is written to a file and then send as input to feminos-daq. Change contents to fit your needs

# if "feminos-daq" is not globally accessible, add it to FEMINOS_DAQ_EXECUTABLE variable
if "FEMINOS_DAQ_EXECUTABLE" not in os.environ:
    feminos_daq_executable = (
        subprocess.check_output("which feminos-daq", shell=True).strip().decode("utf-8")
    )
    os.environ["FEMINOS_DAQ_EXECUTABLE"] = feminos_daq_executable
else:
    feminos_daq_executable = os.environ["FEMINOS_DAQ_EXECUTABLE"]

try:
    subprocess.check_call([feminos_daq_executable, "--help"])
except subprocess.CalledProcessError:
    print("Error: feminos-daq executable not found or not working")
    sys.exit(1)

config_filename = "./feminos-daq-commands"
with open(config_filename, "w") as f:
    f.write(config_contents)

ip = "192.168.10.1"
output_filename = "./output_file.root"
run_time = 30  # seconds

result = subprocess.run(
    [
        feminos_daq_executable,
        "-s",
        ip,
        "--time",
        str(run_time),
        "--skip-run-info",
        "--disable-aqs",
        "--input",
        config_filename,
        "--output",
        output_filename,
    ]
)

# print return code
print(f"Return code: {result.returncode}")

# check output file exists, print size
output_file_size = os.path.getsize(output_filename)
print(f"Output file size: {output_file_size} bytes")

file = uproot.open(output_filename)
tree = file["events"]

for events in tree.iterate(step_size=1):
    events["signal_values"] = ak.unflatten(events["signal_values"], 512, axis=1)

    # signals is a record with fields 'id' and 'data'
    signals = ak.Array({"id": events["signal_ids"], "data": events["signal_values"]})
    events["signals"] = signals

    events = ak.without_field(events, "signal_ids")
    events = ak.without_field(events, "signal_values")

    for event in events:
        for id, data in zip(event.signals.id, event.signals.data):
            print(f"Signal ID: {id}, data: {data}")

    break
