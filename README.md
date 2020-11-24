# Socket Programming

**T.H.Arjun**

**CSD, 2019111012**

## Overview

The requirements in the assignment PDF has been implemented. Below are basic instructions.

## Instructions

- Unzip the zip folder
- Change to the directory 2019111012

```C
cd 2019111012
```

- run

```C
  make clean; make;
```

- This will create necessary binaries in `bin` folder. The binaries will be added to folders `client` and `server` for convenince during testing
- Copy the binaries to the folder for client and server respectively. The code treats the current directory of executable as the disk.
- For testing `server` and `client` folder can be used
- Also settings could be tweaked in `setting.h` if required. The settings put in currently are tested and working. (Buffer size is recommended to be at max 16K since there can be a cap on socket for higher numbers)
- After moving to respective directories give the appropriate user rights to executable.
- Run the binaries
- The server side has no interface.
- The client side has an interface a `CLI` and its usage is very easy.
- It prints the instructions at the start and has further prompts.
- The server and client both print statuses, instructions and errors. Error handling has also been done.
- It is appropriate to start the server first then the client even though there is a prompt in client to try again.

### CLI on Client

### Commands

USAGE:

1. `get <file1> <file2> ... ...` : This gets the files from server directory
2. `exit` : This exits the Client

Cases of CTRL-C , CTRL -D , SIGPIPE etc have been handled appropriately.

### Implementation

**The code for both are commented very well. Please refer the comments.**
