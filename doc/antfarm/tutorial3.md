# A3 Namespaces: 3 environments on one node
## It's like "I'm my own Grandpa" but instead "I'm my own CPU server!"

This demo uses the 9queen (created in the final part of the first
[tutorial](tutorial)) to create a normal local fossil root Bell Labs
terminal - but it can cpu into 9front by dialing itself.

#### Boot 9queen and choose option 4 for interactive boot

	[HOST] qemu -hda 9queen -net nic -net user,hostfwd=tcp::2564-:564,hostfwd=tcp::17010-:17010,hostfwd=tcp::17020-:17020 -cdrom 9front-*.iso

proceed through interactive boot and hit enter for all responses
except as specified:

	tgzfs=tools.tgz
	getrootfs=srv
	
At the next prompt, asking for the path to bootsrv, drop to rc and set
up the cd srv:

	rc
	9660srv -f /dev/sdD0/data boot
	exit

Then continue responding to prompts with enter for the default except
as mentioned:
	
	bootsrv=/srv/boot
	unprivileged=no
	
plan9rc will start the cpurc from the 9front cd and print a few
messages to the console, then return you to the prompt.

#### rerun plan9rc to start up our local Bell Labs terminal.

	mv /srv/boot /srv/front
	. /bin/plan9rc

This runthrough of plan9rc we will answer clear to almost all
questions.  respond "clear" to all prompts until prompted about file
server attach for root fs.  Press enter here to accept the default
valuie of "local".  Continue accepting the defaults until:

	initscript=clear
	rootstart=terminal

At this point the termrc from the labs install to the local disk takes
over and the console becomes a standard bell labs terminal GUI.

## Accessing labs environment and 9front environment from the service namespace

#### cpu or drawterm into port 17020 service namespace

Inside the service namespace you will open two windows.  We will
reroot to a different namespace within each.  In the first we will
reroot to labs namespace

	mv /srv/boot /srv/labs
	rerootwin labs
	service=con
	. $home/lib/profile
	grio -c 0x54321

In the second we will reroot to 9front namespace

	mv /srv/front /srv/boot
	rerootwin -f boot
	service=con
	. $home/lib/profile
	aux/listen1 -t tcp!*!17010 /bin/cpu -R &
	grio

Now there are independent namespaces in each window.  You will notice
that in all 3 namespaces, the "Hub" menu option is connected to the
same hubfs already.

## Cpu to a different namespace on the same machine

#### Inside the labs GUI namespace

	cpu -h localhost
	rio

The end result is like having a separate 9front cpu server on your
grid, but it just happens to be located inside your normal terminal.
If the user has followed the tutorials through this point, it should
be easy to connect to the main hubfs from this namespace and use cpns
to change namespace between the different environments.


