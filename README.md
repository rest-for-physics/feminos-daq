## `feminos-daq`

This is an upgraded version of the original `mclient` program written by Denis Calvet.

### New features

* A prometheus exporter has been added to the mclient program.
  This will provide a way to monitor the program externally (via a slow control system, for example).
  More info in the [Prometheus Exporter](#prometheus-exporter) section.

* `ROOT` has been added as dependency in order to provide a way to store the data in a more efficient way.
  The run and event data will be stored in a root file.
  The root output file should be used instead of the old binary files. More info in the [ROOT output](#root-output)
  section.

* A viewer program has been added to the project. This program will allow to visualize the data stored in the root file
  and can be used to view live data as it's being acquired.
  More info in the [Viewer](#viewer) section.

* An improved CLI has been added to the mclient program using the `CLI11` library.

* The code has been migrated from a pure `C` code to a more modern `C++17` code that assumes it will be compiled for a
  linux target.
  A lot of the code has been removed as it was not required for the `mclient`. The remaining code has been refactored
  to `C++17` but it's still using the `C` style for the most part.

* `CMake` has been added to the project to provide a more modern way to build the project.

### Installation

This project uses `CMake` to build the project. To build the project, run the following commands from the root of the
repository:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/custom/install/path
cmake --build build
```

This will generate the `feminos-daq` executable in the `build` directory.

It is recommended to install the program in a directory present in the `PATH` environment variable so that the
`feminos-daq` and the viewer `feminos-viewer` can be run from any directory.

Installing the program using cmake will also move the python viewer program to the same directory as the `feminos-daq`.

A way to install for all users is to avoid specifying the `CMAKE_INSTALL_PREFIX` so it installs in the default system
path. It's probably necessary to use `sudo` to install the program in a system path.

```bash
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

We make use of `CMakes` `FetchContent` module to download the new required dependencies (CLI11 and Prometheus-cpp).
Only ROOT remains an external dependency (that was not the case in the original `mclient`).

The `feminos-daq` executable is only meant to be compiled for a linux target but the `feminos-viewer` python program can
be run on any platform.

### Usage

To get a list of the available options, run the following command:

```bash
./feminos-daq --help
```

The usage remains mostly the same as the original `mclient`. Most of the original options are still available, but
some (rarely used) have been removed or changed. The most
important changes are:

* `readOnly` mode is now invoked with the `--read-only` flag.

### Prometheus Exporter

The prometheus exporter is a new feature that allows to monitor the `mclient` program externally.
Prometheus is an industry standard for monitoring and alerting, and it's widely used in the industry.
More info about prometheus can be found [here](https://prometheus.io/).

The prometheus exporter is a simple HTTP server that listens on a port (default is 8080) and provides metrics about
the `feminos-daq` program.
After starting the `feminos-daq` program, the metrics can be accessed
at [http://localhost:8080/metrics](http://localhost:8080/metrics).

Accessing this html page may be enough for basic monitoring. For instance a slow control system could be configured to
check the values of the metrics and raise an alarm if something goes wrong.

### `ROOT` output

`feminos-daq` maintains the old binary output format (`.aqs`) for compatibility reasons but also writes data to a root
file.
This new output file is recommended over the old binary files as it's more efficient and easier to read.

The root file will be created in the same directory and with the same name as the binary file. Instead of using subruns,
the data will be stored in a single file.

This file is constantly updated as new data is added.
It can be read as it's being written and a sudden interruption should not corrupt the file.

The layout of this file has been designed so that the file is as small and easy to read as possible.
It does not use dictionaries, so it can be read directly by plain `ROOT` or `uproot`.

Since we wanted to keep the file as simple as possible, it was not possible to replicate the natural structure of the
data: a run with events each event a collection of signals and each signal a collection of 512 values.

Instead, the data is stored in the following way: each event corresponds to an entry in the `TTree`. The signal ids are
stored in a `std::vector` and the signal data is also stored in a single `std::vector<unsigned short>` as opposed to the
more
natural `std::vector<std::vector<unsigned short>>`.
In order to reconstruct the original data, the signal data must be split in chunks of 512 values.
The order of these will be the same as the order of the signal ids.

Storing data in a root file as opposed to the old binary files has several advantages:

* The data is stored in a more efficient way (less disk space is required). This is due to the fact that the data is
  stored in a more compact way and the file is compressed.
  `ROOT` provides some user configuration regarding compression. The default mode of `feminos-daq` is to use a high
  compression algorithm (`LZMA`) but a faster algorithm (root's default) can be selected with the `--compression=fast`
  option.
  From our tests we measured a compression factor between 2.5 and 8 with respect to the old binary files.
* The data is more straightforward to read and write. The data can be read and written using `ROOT` or `uproot` without
  the need for a custom reader/writer. This helps unfamiliar users to access the data more easily.

#### Frames Queue

The data is not written to the root file as it arrives (in contrast to the binary files).
Instead, since writing root files is relatively slow, the frames (the data being sent from the electronics) are stored
in a container which is periodically emptied by a separate thread that writes the data to the root file.

This reduces the overhead of writing to the root file and allows the program to keep up with the data rate.

However, if the rate at which data arrives is too high, the program will not be able to keep up and the frames queue
will begin to fill up.
If the queue reaches a certain size, the program will stop. The size of the queue is displayed periodically in the
terminal next to the speed of the data acquisition as long as the queue is above a certain size.

In general the user shouldn't worry about this as the queue takes a long time to fill up even for high data rates.
The only scenario where this could be a problem is on high intensity calibrations.
In this case the user can select the `--compression=fast` option which will significantly speed up the rate at which the
queue is emptied. Remember that this will increase the size of the root file so it's not recommended unless it's
unavoidable.

#### Processing the ROOT files

As mentioned before, the root file can be read using `ROOT` or `uproot`.
It's also possible to use the root file ("feminos-root") as input for a `REST-for-Physics` analysis using
the [TRestRawFeminosRootToSignalProcess](https://github.com/rest-for-physics/rawlib/blob/master/inc/TRestRawFeminosRootToSignalProcess.h).
This process has no options and will read the root file and output a `TRestRawSignalEvent` for each event in the input
feminos-root file.

Even though the `--compression=fast` files and the regular more compressed files are different in size and take
different
times to write, we have measured no difference in the time it takes to process the files with the
`TRestRawFeminosRootToSignalProcess`.

### Viewer

`feminos-viewer` is a Python GUI application that allows to visualize the data stored in the root file.
`tkinter` is used for the GUI, `uproot` to read the data from the root file and `matplotlib` to plot the
data.

The required dependencies are listed in `viewer/requirements.txt`. To install them, run the following command:

```bash
python3 -m pip install -r viewer/requirements.txt
```

To run the viewer program, use the following command:

```bash
python3 viewer/feminos-viewer.py
```

or you can just run `feminos-viewer` from any directory if the cmake install was used.

![feminos-viewer-screenshot](https://private-user-images.githubusercontent.com/35803280/362641091-2ee62e73-83e7-4be2-a7cd-ac37779de5c7.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MjQ5NTc4OTEsIm5iZiI6MTcyNDk1NzU5MSwicGF0aCI6Ii8zNTgwMzI4MC8zNjI2NDEwOTEtMmVlNjJlNzMtODNlNy00YmUyLWE3Y2QtYWMzNzc3OWRlNWM3LnBuZz9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNDA4MjklMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjQwODI5VDE4NTMxMVomWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPWE2MzlkMjQ1YTgyM2Q4ZGY4ODAxZjgzMWIyNjc2NGIxZDA4ZWEzZmIxNjljMzMwZTMzNWIzMTM4NjBhZTM5YWEmWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0JmFjdG9yX2lkPTAma2V5X2lkPTAmcmVwb19pZD0wIn0.JU1rU09TNLZu6zbEMwbs6uCCFIhmv8GBW5husEntMpc)

There are three ways to open a file:

- The `Open Remote File` button will pop up a dialog to enter the URL of the file to open. This can be any URL supported
  by `uproot`, such as `https`, `s3` or even `ssh`. This means that the viewer can be run on a local
  computer and the data from a remote computer (accessible by ssh) can be displayed.
- The `Open Local File` button will open a file dialog to select a file from the local filesystem.
- The `Attach` button will attempt to find a running instance of the `feminos-daq` program and open the file being
  written by it. This is the recommended way to open a file if the data is being written in real time. Remember that the
  viewer uses the data from the root file, so it's not possible to live-view data when operating in `--read-only` mode.

The viewer has an option to select a readout corresponding to the data being read.
The readout is just a mapping between signal ids (ids of the signals in the root file) and the physical channel they
correspond to given by the type (X or Y) and the position.

The readouts are hardcoded in the viewer program. In order to add a new readout create a pull request with the new
readout mapping using the existing ones as guidelines. In the `viewer` directory there is a root macro that can be used
to generate the mappings from a `REST-for-Physics` readout.

There is an event menu to navigate through the events. The `Waveforms Outside Readout` option can be selected to plot
all signals, even those that are not part of the readout.

The `Auto-Update` option will periodically reload the file and select the latest event. This is particularly useful
when the file is being written in real time.

The `Observables` mode will display and compute some observables from the data such as an energy estimate or the channel
activity.
This computation is done by the viewer program so it may take significant time to produce histograms with a high number
of counts. The computations are only performed as long as the `Observables` mode is selected (so it might be a good idea
to leave this mode selected when performing a long acquisition run). The `Auto-Update` option will periodically refresh
the observables so it's recommended to enable it when using the `Observables` mode.

### Automatic syncing of output to remote server

It is a frequent requirement that the data acquired by the `feminos-daq` program is stored in a remote server.

This repository includes a script (`/scripts/feminos-daq-sync.sh`) that can be used to sync the data directory to a
remote host using `rsync`.

```bash
feminos-daq-sync.sh local_data_directory/ remote_user@remote_host /remote_data_directory/
```

It is recommended to setup a systemd service to run this script.
The user running the service should have password-less ssh access to the remote host.

`/etc/systemd/system/feminos-daq-sync.service`:

```bash
[Unit]
Description=File Sync Service
After=network.target

[Service]
ExecStart=/usr/local/bin/feminos-daq-sync.sh /home/user/data/ user@remote /remote/storage/data/
Restart=always
User=useriaxo

[Install]
WantedBy=multi-user.target
```

Some useful commands (in case the script gets stuck, for example) to run in the `/scripts/` repository:

- `journalctl -u feminos-daq-sync.service`
  This will show the latest executions of the script. This can be useful to check the last time it was correctly run.
- `systemctl status feminos-daq-sync`
  This will show the status of the script. If there is an error it will show "active: failed".
- `systemctl restart feminos-daq-sync`
  When that happens restarting the script might solve the issue.
- `systemctl stop feminos-daq-sync`
  In case you want to stop the script.
