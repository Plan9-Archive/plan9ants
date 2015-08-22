# Tutorial for using 9worker.gz raw VM image
### Walkthrough done with Qemu in Debian Wheezy
#### commands shown are executed inside the vm except where shown as

	[HOST] wget http://9gridchan.org/9worker.gz
	[HOST] gunzip 9worker.gz

When launching the qemu 9queen fileserver, you may wish to increase
the default memory slightly from 128m to 256m.  For convenience we
will be specifying drives using -hda and -hdb.This is usually slow.
You probably want to specify drives differently.  The following
example is for hda(drive 1), change index=0 to index=1 for hdb(drive
2).  More info on setup can be found at the [setup](setup) page.

	[HOST] qemu -drive file=9worker,media=disk,index=0,cache=writeback

## using 9worker standalone -- boot option 1 ##

	[HOST] qemu -hda 9worker -net nic -net user,hostfwd=tcp::17060-:17060

#### Choose option 1, the default, at the boot menu, then at the prompt type

	gui

Now you can work in the rootless environment.

#### To give a window access to the full distribution, use the command

	addsources

Then look at the ns of that window.  You can use any binary from the
full distribution.  This is just a demonstration, please don't place a
lot of unneeded load on sources by making it your root frequently with
this method.  We can use this namespace alteration to demonstrate
namespace modifications via /proc.

#### Open three windows. In the first, do

	echo $pid
	addsources

In the second, just echo $pid to find it's pid. Then from the third window:

	cpns -t -r sourcespid otherpid
	cpns -r sourcespid otherpid

Now check out the namespace of second window.  It has become a copy of
the process in the first window.

#### Hubfs shells in grio

The modified version of rio "grio" has new menu options.  Use the
"Hub" menu option to sweep out a new window in the same way as with
New.  Enter some commands in this shell, then delete the window.  (Do
not type exit in the shell.) Now open another window with the Hub
option.  You are connected to the same shell.  This is
[hubfs](http://www.plan9.bell-labs.com/wiki/plan9/hubfs/index.html)
which uses 9p to multiplex pipelike files.  It can be used like
screen/tmux.

#### Drawterm in to the mini-cpu server

	[HOST] drawterm -a dead.ip -c 'tcp!your.local.ip.address!17060' -u glenda

When prompted for a password enter

#### rootless

After a moment, you should be inside the mini-cpu server environment.
Look around in /bin to see what can be done with just the worker
alone.  You may notice that the Hub menu doesn't connect to the same
shells as before.  Look in /srv and you will see both /srv/hubfs and
/srv/riohubfs.glenda.  By choosing which is mounted at /n/hubfs, you
can start a new sub-rio connected to whichever io hubs you wish.  This
feature will be used more in the demos with multiple systems.

## using 9worker with a 9front live cd -- boot option 2 

(Note that the Bell Labs .iso works the same way for this, other than
one command flag as noted below.)


	[HOST] qemu -hda 9worker -cdrom 9front-*iso -net nic -net user,hostfwd=tcp::17060-:17060

#### hit enter twice to accept defaults at prompts
The boot process will put you inside a live cd environment.  To get to
the independent controlling namespace, cpu or drawterm in.


	[HOST] drawterm -a 192.168.244.244 -c 'tcp!your.local.ip.address!17060' -u glenda

(for drawterm as hostowner you can use a meaningless ip address)

From inside drawterm explore the basic rootless environment.  One
thing to do is sweep out a new window using the Hub menu to start a
new shell.  Type ns and some other commands.  You can connect to the
same hub again using the same menu item.


#### To connect to 9front cd namespace

	rerootwin -f boot
	service=con
	. $home/lib/profile
	grio -s

If you are using the Bell Labs .iso rather than 9front, just remove
the -f flag from the rerootwin command in the above example.

You have now rerooted into the 9front namespace within the service
namespace.  If you use the Hub menu now to make a new window you will
be back in the hubfs ns.  The different namespaces between the cd
environment and the rootless environment provide a good opportunity to
test cpns between different processes.


## Using 9worker and 9queen together -- 2 nodes
#### Example setup given using a completely fresh install from bell labs iso
(This shows how to create the worker and queen images.  If you
download the preinstalled images, you can skip the first two steps.)

	[HOST] qemu-img create -f raw 9queen 2G
	[HOST] qemu-img create -f raw 9worker 16m
	[HOST] qemu -hda 9queen -hdb 9worker -cdrom plan9.iso

#### Install plan 9 from the .iso and then reboot and now inside

	DNSSERVER=8.8.8.8
	ip/ipconfig
	ndb/cs
	ndb/dns -r
	mkdir ants
	9fs sources
	dircp /n/sources/contrib/mycroftiv/rootlessnext ants
	cd ants
	build isoinstall
	build worker
	cd cfg
	stockmod
	fshalt

(This is the exact process by which the two images were created.)
#### Now we are ready to use both together

	[HOST] qemu -hda 9queen -net nic -net user,hostfwd=tcp::2564-:564,hostfwd=tcp::17007-:17007,hostfwd=tcp::17020-:17020
	[HOST] qemu -hda 9worker -net nic -net user,hostfwd=tcp::2567-:567,hostfwd=tcp::17010-:17010,hostfwd=tcp::17060-:17060

Note that these port redirections assume you are making use of another
layer of port redirection with iptables to avoid the annoying UNIX
"privileged ports" issue.  For more information and examples, the wiki
page [installing plan 9 on
qemu](http://www.plan9.bell-labs.com/wiki/plan9/Installing_Plan_9_on_Qemu/index.html)
has details.


#### choose option 2 on 9queen boot (cpu server)
#### choose option 3 on 9worker boot and then choose tcp
#### enter tcp boot ips of 9queen (your local machine ip probably)

	[HOST] drawterm -a localhost -c localhost -u glenda

	echo '' >>/lib/ndb/local
	echo 'ip=your.local.ip sys=filesrv authdom=rootless auth=helix' >>/lib/ndb/local
	echo -n refresh >/net/cs
	import -c filesrv /srv /n/fsrv
	mount -c /n/fsrv/hubfs /n/hubfs
	grio -s -c 0x999999

Now you can work in a normal cpu root but your hubfs menu has
persistent access to the service namespace on the fileserver.


#### Access the service namespace on the tcp cpu

	[HOST] drawterm -a localhost -c 'tcp!localhost!17060' -u glenda

#### Access the service namespace on the file server

	[HOST] drawterm -a localhost -c 'tcp!localhost!17020' -u glenda

This environment with two machines is also good for testing rerootwin,
cpns of remote processes, and many other aspects of ANTS not covered
in this brief tutorial.


## Complex ANTS usage demo: 2 nodes, 5 namespaces

This example makes use of the previously configured two nodes, and
then the Bell Labs and the 9front live cds.  We will make namespaces
available rooted to disk and both cds as well as the service
namespaces on each node, then we will connect shells from each
namespace to a shared hubfs available in each environment.  It will
also show rewriting of the namespace of remote processes and rerooting
to namespaces on remote hosts.  Learn more about the colony in
[tutorial2](tutorial2).
