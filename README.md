Relevant changes to the original version

* When using "exec" command with a command file with name different from "ped,start,runTCM", which are the values to be used for pedestal, initialization and TCM, we will be prompted for some run conditions.

* If properly configured, an automatic elog entry will be generated.
  * If the runTag contains the word "Test" or "test", this feature will be skipped.
  * It is important that the e-log contains a field named detector, and the detector corresponds with one of the options at the e-log.

* A shared memory resource will be created when using "shareBuffer" option in mclient.
   Usage : ./mclient shareBuffer

   * This buffer can be accessed by other processes, and in particular by TRestSharedMemoryBufferToRawSignalProcess.

* A read only option. Actually, this option will be mostly used together with shareBuffer or very basic tests.
   Usage : ./mclient readOnly
   Usage : ./mclient readOnly shareBuffer

   * The data flow received will not be dumped to disk.

* A new empty file will be generated at FILES_TO_ANALYSE_PATH to indicate when a particular RUN has completely finished.

   * The generated file under FILES_TO_ANALYSE_PATH will have extension *.endRun*. And it will use the same file name as the last file written.

