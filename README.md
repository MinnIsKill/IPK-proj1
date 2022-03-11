# IPK - project #1
Project #1 for VUTBR FIT - IPK (Computer Communications and Networks)

## Brief
This project's primary goal was to implement a minimalistic C/C++ server using the HTTP protocol, which 
would provide some basic functionality.

## How to run
Part of the repository is a very simple Makefile which can be run from the command line simply by 
using the 'make' command and will provide you with an executable.

```
$ make
$ ./hsinfosvc [localport]   (where 'localport' is a sequence of numbers of your choosing, e.g. 12345)
```
Now you have your server up and running. To close it, use the keyboard shortcut 'Ctrl+C'.
Alternatively, add '&' as a third argument to open a server in the background.
Next, a GET request can be sent to the server, for example by using curl:
```
$ curl http://localhost:12345/[command]    (more on available commands below)
```
The same information can be gotten by using a browser as well, search: 'http://localhost:12345/[command]`

## Supported commands
* **hostname:**  returns name of server host
* **cpu-name:**  returns name of server host's CPU
* **load:**      return server host's CPU's current load

### Author
Vojtěch Kališ, xkalis03@stud.fit.vutbr.cz