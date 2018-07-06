Relevant changes to the original version

* When using "exec" command with a command file with name different from "ped,start,runTCM", which are the values to be used for pedestal, initialization and TCM, we will be prompted for some run conditions.

* If properly configured, an automatic elog entry will be generated.
  * If the runTag contains the word "Test" or "test", this feature will be skipped.
  * It is important that the e-log contains a field named detector, and the detector corresponds with one of the options at the e-log.

* A shared memory resource will be created when using "shareBuffer" option in mclient.
   Usage : ./mclient shareBuffer

   * This buffer can be accessed by other processes, and in particular by TRestSharedMemoryBufferToRawSignalProcess.
   * When mclient program is terminated by clean exit or by CTRL-C the shared resources will be removed.

* A read only option. Actually, this option will be mostly used together with shareBuffer or very basic tests.
   Usage : ./mclient readOnly
   Usage : ./mclient readOnly shareBuffer

   * The data flow received will not be dumped to disk.

* A new empty file will be generated at FILES_TO_ANALYSE_PATH to indicate when a particular RUN has completely finished.

   * The generated file under FILES_TO_ANALYSE_PATH will have extension *.endRun*. And it will use the same file name as the last file written.

* When launching a command file (using exec) that is not named "start", "ped" or "runTCM" a command will be sent to the cards to reinitialize the event counter and timestamp.
   * This is important in order to construct the timestamp using the file creation timestamp and the internal electronics clock sampling.
   * An alternative way (future implementation required) would be to use directly the system time.

* A new option has been added to support TCM event build. We need to specify in the command line an argument to let know mclient we are using a TCM.
  Usage : ./mclient shareBuffer tcm

* We can now specify several options using an argument RST or ST.
  Usage : ./mclient RST # Equivalent to readOnly, shareBuffer and tcm
  Usage : ./mclient ST # Equivalent to shareBuffer and tcm
