# Advanced Advanced Namespaces: 5 namespaces, 2 nodes
## What happens in this demo:

* Two machines boot, the second as a tcp cpu from the first machine's disk fs
* Each machine starts an additional namespace on the local display using a live cd
* Now there are 5 independent namespaces: 1 disk root, 1 labs cd, 1 9front cd, 2 ram/paq service ns
* Next the disk and cd namespace connect to a shared hubfs and create persistent shells
* This makes persistent shells in every namespace available in every other namespace
* The tcp cpu imports the /proc from the other server and locates a 9front process and a service ns process
* The tcp cpu uses cpns to rewrite the namespace of the other machine's ramrooted process to be inside the 9front namespace
* The user on the labs machine imports and mounts the 9front cd and uses rerootwin to enter that namespace
* The five independent namespaces hosted on the two nodes all coexist and the user can share resources between them as needed
* The user can enter any namespace with rerootwin
* Within each namespace, the user can access and control the other namespaces via hubfs
* The disk and cd namespaces behave normally for the user and use their normal default configuration

This demo shows a more elaborate use of ANTS.  This will show how
multiple namespaces on different machines can be created, connected,
and entered freely by the user.  The exact setup used - a hard drive
and 2 cds - is not intended as a practical model - these could be any
root fses of any type, local or network, on any medium.  This
walkthrough was constructed to use cd .iso images because they are
consistently available and demonstrate the principles, which can be
applied to any root fs.

#### Not getting lost

Interacting with this many namespaces at once can be disorienting even
for experienced Plan 9 users.  Here is a summary of how the namespaces
in this demo are accessed:

* Cpu port 17010 accesses a tcp root namespace with a disk fs from the other server
* Cpu port 17020 accesses the service namespace of the fileserver providing the disk root
* Cpu port 17060 accesses the service namespace of the tcp boot cpu
* The console of the file server will be taken over by a 9front live cd terminal namespace
* The console of the tcp cpu will be taken over by a Bell Labs live cd terminal namespace

The machine which boots first (9queen) and runs the disk fossil will be called
###filesrv
and the machine which boots second (9worker) as a tcp cpu will be called
###helix

From within these namespaces, additional connections will be made
using hubfs.  The hubfs will allow shells from each namespace to be
accessed from the same hub.  The user can also move between namespaces
using the rerootwin script.  The demo will put you in namespaces where
you cpu to machine A, enter a namespace hosted on machine B, and then
use hubfs to access an entirely different namespace C.

The fact that you can "get lost" in all this is a feature, not a bug -
the transparency and seamlessness of the environment is the whole
point!  This particular example may seem contrived at first - a
"namespace maze" - but it was created to demonstrate the principles
involved.  The ability to create namespaces, edit them, and move
between them is generally useful and the essence of Plan 9.  The
techniques in this demo can be applied to standard configurations such
as venti -> fossil -> tcp cpu -> terminal.

If you have NOT already used these 2 machines for the previous [tutorial](tutorial) there is an additional step that is needed.

#### extra config performed on helix (9worker) if not performed previously

	echo '' >>/lib/ndb/local
	echo 'ip=your.local.ip.addr sys=filesrv authdom=rootless auth=helix' >>/lib/ndb/local
	echo -n refresh >/net/cs

This is necessary to allow helix to dial and auth to filesrv for
import.  Note that the information is being added to the filesrv disk
but the helix cs is being refreshed with the information.

One tip: on the 9queen=filesrv machine, add the line sysname=filesrv
to the common section of the plan9.ini file.  That way, the machines
won't think they have the same name, which makes it much easier to
get confused.  Another note about this tutorial - because it bounces
back and forth between 5 different 'namespace portals' the
drawterm/cpu commands are not written out explicitly.  Each section of
commands uses the bolded header to identify which of the five
namespaces the given commands are to be executed in.

## Start by booting both nodes in the manner of the earlier tutorial
#### boot 9queen with menuitem 2 and 9front cdrom attached
	[HOST] qemu -hda 9queen -net nic -net user,hostfwd=tcp::2564-:564,hostfwd=tcp::17007-:17007,hostfwd=tcp::17020-:17020 -cdrom 9front-*.iso
#### boot 9worker with menuitem 3 and bell labs cdrom attached
	[HOST] qemu -hda 9worker -net nic -net user,hostfwd=tcp::2567-:567,hostfwd=tcp::17010-:17010,hostfwd=tcp::17060-:17060 -cdrom plan9.iso
#### cpu to worker port 17010

You have cpu'd into a totally conventional tcp boot cpu namespace.

#### Now start an additional cd namespace on each machine

These commands are entered on the console of each virtual machine,
which will be referred to as the "gui" display following.

	mv /srv/boot /srv/diskboot #on filesrv
	9660srv -f /dev/sdD0/data boot
	interactive=yes
	. /bin/plan9rc

change the first line on helix to:

	mv /srv/boot /srv/tcpboot #on helix cpu

While re-executing the plan9rc, enter "clear" for all the options (x6)
until the "getrootfs" option, and then answer "srv".  For the next
question, hit enter to accept the default /srv/boot.  Then "clear" for
next answer, and for "rootstart" choose "terminal".

This will start a new namespace rooted on the live cd on each machine.
The existing cpu namespace is unchanged.

#### Now move the original /srv/boot back. On filesrv gui

	mv /srv/boot /srv/front
	mv /srv/diskboot /srv/boot

#### Repeat this on helix gui

	mv /srv/boot /srv/labs
	mv /srv/tcpboot /srv/boot

#### back inside the front cd namespace on the filesrv gui

	mount -a /srv/bootpaq /bin
	mount -c /srv/hubfs /n/hubfs
	hub
	%local front

This mounts the hubfs and shares a shell from the current 9front namespace to it.

#### In helix cpu tcp namespace (port 17010)

	import -s hubsrv -c filesrv /srv /n/fsrv
	mount -c /n/fsrv/hubfs /n/hubfs
	hub
	%local tcphelix

#### Inside labs cd namespace on helix (gui display)

	mount -a /srv/bootpaq /bin
	mount /srv/hubsrv /n/fsrv
	mount -c /n/fsrv/hubfs /n/hubfs
	hub
	%local labs

#### now cpu/drawterm to service namespace port 17060 of helix

	import filesrv /srv /n/fsrv
	mount -c /n/fsrv/hubfs /n/hubfs
	ls /n/hubfs
	hub
	%attach front

Both machines can now make use of all the namespaces easily via the shared hubfs.

## modifying the ns of remote processes

We will demonstrate modifying the namespace of processes on a remote box via /proc.

#### cpu to filesrv 17020 service namespace

Open a new window in grio

	echo $pid

#### In filesrv 9front cd gui namespace, repeat in another new shell

	echo $pid

#### in helix 17060 service ns

	import filesrv /proc

#### Copy the ns of the 9front cd shell onto the rootless shell ns 
(This is still done from the helix 17060 service ns.)

	cpns -t -r 9frontpid servicepid
	cpns -r 9frontpid servicepid

#### Go back to filesrv 17020 and check the changed ns in the first shell

	theo
	games/packet

## Rerootwin to an alternate remote ns

This example is similar to the one-machine example with a live cd, but
it reroots to the cd namespace hosted on the other machine.

#### In helix 17060 ns

	mntgen -s slashmnt /mnt && chmod 666 /srv/slashmnt
	mntgen -s mntexport /mnt/exportfs && chmod 666 /srv/mntexport
	import filesrv /srv /n/fsrv
	mount -c /n/fsrv/front /n/front
	srvfs localfront /n/front
	rerootwin -f localfront
	service=con
	. $home/lib/profile
	import filesrv /srv /n/fsrv
	mount -c /n/fsrv/hubfs /n/hubfs
	grio

Now we are inside the 9front cd namespace, served to us from the 'tcp
cpu' and we have access to all the other namespaces via the Hub menu.

## What else is part of the Advanced Namespaces Tools?

Many uses of ANTS are left out of these tutorials.  Most important for
practical use are the venti and fossil tools.  In the author's usage,
the multiple namespaces are not attached cds - they are independent
chains of venti-fossil servers.  The ventiprog and cpsys(not to be
confused with cpns) scripts back up data between ventis and make the
same rootscore available from different fossils.  This means that
additional independent namespaces with the same data are available for
backup and for testing or simple parallelization.

The ANTS boot system (the plan9rc, ramskel, and initskel scripts
primarily) has many options and aims to be backwards-compatible with
all existing x86 pc boot options.  It is possible to attach to a root
fs in new ways, such as connecting to a u9fs server and then choosing
a particular subdirectory to root from rather than using the root of
the entire u9fs system.

ANTS has several other scripts and use modes.  The scripts getdevs and
savedevs are related to rerootwin and perform the "device saving and
retrieval" without using newns to enter a new independent namespace.
They are often useful in combination with shell s inside hubfs, to
allow them to track window width correctly.  Scripts such as addwrroot
attach to file or cpu servers to make use of their resources without
rerooting fully.  Hubfs is general purpose network piping in addition
to persistent shells and has several tricks of its own.

The full Advanced Namespace Tools website at
[http://ants.9gridchan.org](http://ants.9gridchan.org) (also available
with 9fs ants.9gridchan.org or ftpfs ants.9gridchan.org) has the
source code, compiled kernel and tools, and much more documentation
including the full Attack of the Giant Ants!  paper.
