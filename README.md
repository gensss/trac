# trac

**Trac is a simple process tracker based on the linux kernel mechanism of [proc events over netlink](http://bewareofgeek.livejournal.com/2945.html).**

This was made for fun so expect long periods without updates.
Compiling it will yield a daemon named tracd, and a client named trac.  
Tracd should, ofc, be ran in the background for it to work.

**Trac usage (needs priviledges to run):**

tracd &  #for now
(or in another shell)

trac add *identifing_name* *program_to_run* *programs_arguments*  
example:  
trac add "a shell" /bin/bash -i  

trac list *identifing_name*  
example:  
trac list "a shell"  
8101: /bin/bash  
8193: /usr/bin/htop  


It is not yet ready for everyday usage.  
For example it leaves a domain socket fd /dev/shm/trac that needs to be manually removed before restarting the daemon.

**The things that need to be done to make it usable:**  
-Another place to store the UNIX domain socket fd used for IPC, deleting it when done and checking it when starting.  
-Add more formating options to the client. Just a string of PID-s being the default.  
-Add the usual checks. Does it have priviledges, is the kernel compiled with CONFIG_PROC_EVENTS, error code checking, etc.  
-Make tracd deamonize.  
-Make tracd set its nice to be lower to avoid netlink buffer overflow in case of fork bombs on an overloaded system.  
-Make tracd use poll when receiving client messages, to avoid a bad client locking it.  
-Add a check when adding PIDs to a list. Currently it overflows if there are more then 1024 PIDs under one identifier.  
-Find out how threads go into this. (should work now, but i need to do a bit more research)  

**The extra things:**  
-Change from a raw array of pids to a linked list or something, to allow constructing a "genealogical" tree of processes.  
-Add the ability to send a PID of an already running process, instead of just starting a new one.  
-Add an option to list all tracked groups of processes.  
-Add the ability to send a signal() to a group of processes.  
-Many more that i can't remember right now.  

Oh, yea. I should move the debug printf()'s to a -debug argument (or just -v). :)

If you find this usable, send me a msg.  
First time i'm using github so idk how, maybe open a bug request or something ?
