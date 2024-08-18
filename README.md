## `mclient`

This is a modified and extended version of the original `mclient` program.

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
cmake -S . -B build
cmake --build build
cmake --install build
```

This will generate the `mclient` executable in the `build` directory.

We make use of `CMakes` `FetchContent` module to download the new required dependencies (CLI11 and Prometheus-cpp).
Only ROOT remains an external dependency (that was not the case in the original `mclient`).

### Usage

To get a list of the available options, run the following command:

```bash
./mclient --help
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
the `mclient` program.
After starting the `mclient` program, the metrics can be accessed
at [http://localhost:8080/metrics](http://localhost:8080/metrics).

Accessing this html page may be enough for basic monitoring. For instance a slow control system could be configured to
check the values of the metrics and raise an alarm if something goes wrong.

### `ROOT` output

The `ROOT` output is a new feature that allows to store the data in a more efficient way.

It is enabled by default and a root file will be created in the same directory as the binary file.
Instead of using subruns, the data will be stored in a single file. This file is constantly updated as new data is
added.
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
  `ROOT` provides some user configuration regarding compression. We chose a moderately high compression level to keep
  the file size small while still allowing for fast reading and writing.
  From our tests we measured a compression factor of `~x8` with respect to the old binary files.

* The data is more straightforward to read and write. The data can be read and written using `ROOT` or `uproot` without
  the need for a custom reader/writer. This helps unfamiliar users to access the data more easily.

However, storing the data in a root file requires more processing power and memory than the old binary files, so it
could
potentially slow down the data acquisition.
From our preliminary tests, we have observed a slowdown (factor of 2-3) with respect to the old binary files when
performing acquisition at a very high rate (~10 MB/s in binary mode) with noisy detectors and saving hundreds of
channels per event.
We do not expect any slowdown for background runs and only expect some slowdown for calibration runs with a very high
rate.

### Viewer

The viewer program is a new feature that allows to visualize the data stored in the root file.
It's a simple `tkinter` program that uses `uproot` to read the data from the root file and `matplotlib` to plot the
data.

The required dependencies are listed in `viewer/requirements.txt`. To install them, run the following command:

```bash
python3 -m pip install -r viewer/requirements.txt
```

To run the viewer program, use the following command:

```bash
python3 viewer/viewer.py
```

The viewer program will open a window with some buttons. The user can select a file to view its contents.

There is an option to view the data in real time as it's being written.
The `Attach` button will attempt to find the filename of the root file being written by the `mclient` program (using the
prometheus metrics page).
If the filename is found, the viewer will start reading the file and plotting the data in real time.
The `Reload` button can be used to manually refresh the data.

**Note**: The live data feature reads data from the root file, so unfortunately it won't work when using
the `--read-only` mode.

# - OLD DOCS -

### Compiling the mclient executable

Quick recipe to generate the `mclient` executable

```
cd projects/minos/mclient/linux
make
```

If everything went smooth, the binary will be placed at `projects/bin/minos/linux/`.

A good option is to create a symbolic link to that executable wherever you want to use it.

#### Setting up the DAQ environment

In order to mclient from FeminosDAQ to work properly few environment variables must be defined in the system.

The file `loadDAQ_EnvVars.sh` will serve as a template to help definning those variables.

All these varibles are needed, values could be changed.

The best practice is to modify that file and add the following line to your `.bashrc`, or similar.

```
source $HOME/git/FeminosDAQ/loadDAQ_EnvVars.sh
```

assuming your FeminosDAQ directory lies under `$HOME/git`.

#### Relevant changes to the original version

* When using "exec" command with a command file with name different from "ped,start,runTCM", which are the values to be
  used for pedestal, initialization and TCM, we will be prompted for some run conditions.

* If properly configured, an automatic elog entry will be generated.
    * If the runTag contains the word "Test" or "test", this feature will be skipped.
    * It is important that the e-log contains a field named detector, and the detector corresponds with one of the
      options at the e-log.

* A shared memory resource will be created when using "shareBuffer" option in mclient.
  Usage : ./mclient shareBuffer

    * This buffer can be accessed by other processes, and in particular by TRestSharedMemoryBufferToRawSignalProcess.
    * When mclient program is terminated by clean exit or by CTRL-C the shared resources will be removed.

* A read only option. Actually, this option will be mostly used together with shareBuffer or very basic tests.
  Usage : ./mclient readOnly
  Usage : ./mclient readOnly shareBuffer

    * The data flow received will not be dumped to disk.

* A new empty file will be generated at FILES_TO_ANALYSE_PATH to indicate when a particular RUN has completely finished.

    * The generated file under FILES_TO_ANALYSE_PATH will have extension *.endRun*. And it will use the same file name
      as the last file written.

* When launching a command file (using exec) that is not named "start", "ped" or "runTCM" a command will be sent to the
  cards to reinitialize the event counter and timestamp.
    * This is important in order to construct the timestamp using the file creation timestamp and the internal
      electronics clock sampling.
    * An alternative way (future implementation required) would be to use directly the system time.

* A new option has been added to support TCM event build. We need to specify in the command line an argument to let know
  mclient we are using a TCM.
  Usage : ./mclient shareBuffer tcm

* We can now specify several options using an argument RST or ST.
  Usage : ./mclient RST # Equivalent to readOnly, shareBuffer and tcm
  Usage : ./mclient ST # Equivalent to shareBuffer and tcm
