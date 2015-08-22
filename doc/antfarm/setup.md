# Setup and alternatives for VM images
## Qemu and Drawterm

The software needed on your system to use the ANTS vm image is the Qemu emulator and Drawterm. Linux and other unix variants usually have Qemu as part of their package repository. The official site is [http://qemu.org](http://qemu.org). Drawterm is located at [http://swtch.com](http://swtch.com) and allows graphical access to a Plan 9 system.

## Full sample VM command lines for 9queen and 9worker:
#### 9queen with 256megs memory all needed redirs and 9front iso attached

	qemu -m 256 -drive file=9queen,media=disk,index=0,cache=writeback -net nic -net user,hostfwd=tcp::2564-:564,hostfwd=tcp::17007-:17007,hostfwd=tcp::17020-:17020 -cdrom 9front-*.iso &

#### 9worker with all needed redirections and Bell Labs iso attached

	qemu -drive file=9worker,media=disk,index=0,cache=writeback -net nic -net user,hostfwd=tcp::2567-:567,hostfwd=tcp::17010-:17010,hostfwd=tcp::17060-:17060 -cdrom plan9.iso &

## VNC access

If you would like use vnc to access the VM rather than Drawterm this is possible, but will require a bit of extra setup within the vms. Here is how to start a vnc server from a boot of 9worker to the GUI:

	echo '' >>/lib/ndb/local
	echo 'ip=my.local.ip sys=helix authdom=rootless auth=my.local.ip' >>/lib/ndb/local
	factotum
	cs
	echo 'key proto=p9sk1 dom=rootless user=glenda !password=rootless' >>/mnt/factotum/ctl
	vncs

It will be necessary to add forwarding of port 5900 to the vm. If you have difficulty connecting, adding the -v flag to vncs will produce debugging output to see if the incoming calls are being received.

#### Using VNC with the tutorials

If you are substituting vnc for drawterm while using the tutorials, it will be necessary to forward additional ports and run multiple vnc servers. Each "cpu access port" referred to in a tutorial needs a vncs running in an equivalent namespace. To set this up, just run a new instance of vncs using a different display and port (ex -d :3 for port 5903) in each namespace.

## CPU from Plan 9 

If you already have a Plan 9 system or systems running, you can certainly use CPU instead of Drawterm to connect to the vm images. Just add a line like this to your /lib/ndb/local

	ip=ip.of.vm sys=helix auth=ip.of.vm authdom=rootless

And then

	echo -n refresh >/net/cs

Now you can use cpu -h tcp!helix!port -u glenda to connect to the vm images. Adjust the port number as needed.

## Telnet will never die

Good old telnet. If you want access to your system from absolutely anything that can run a network interface and to get connectivity with zero additional hassles, telnet still gets it done. To provide anonymous access as none to anyone dialing in:

	listen1 -tv tcp!*!21212 /bin/telnetd -a -u none

Telnet to port 21212 to connect. If you are on a secure network you can allow automatic accesss as the full user account this way:

	listen1 -tv tcp!*!21212 /bin/telnetd -t -u glenda

## Qemu networking

This is covered in a lot of places around the net, including at the Bell Labs wiki, so I won't go into a lot of detail. These tutorials are written with user mode port forwarding. This is simple and easy. You can also setup other networking methods to give the VMs their own independent IP. One issue that comes up a lot in unixes is privileged ports. For these demos, use of the auth port (567) is sometimes necessary and you generally don't want to run a VM as root if not needed. You can use your host os to work around this if a tool like linux iptables is available. Execute this as root:

	iptables -t nat -A PREROUTING -p tcp --dport 567 -j REDIRECT --to-port 2567
	iptables -t nat -A OUTPUT -o lo -p tcp --dport 567 -j REDIRECT --to-port 2567

Then when using qemu, redirect external port 2567 to vm port 567. The examples in the tutorial are written this way. You only need to be root to run the iptables commands, and the vm can run unpriveleged. To everything that dials in, and inside the VM, the ports seem standard, the redirection is invisible.

## Troubleshooting

I was hoping you wouldn't get to this section. The most common issues are usually involved in setting up the vm and networking on the host os. Using netstat in both the host os and inside the vm is helpful to make certain that listeners are listening, ports are open, and connections are able to happen. A tool like netcat can also be helpful in checking that a connection can actually be dialed. 

Drawterm is currently broken in freebsd and perhaps other bsd variants.

Qemu user-mode networking has a dns bug in some versions but the vm image works around this with the use of 8.8.8.8 Google public DNS.

If things seem not to be working when following the tutorials, take a minute to make sure you understand where the commands need to be typed. The tutorials often involve typing a few commands in one window, then a few commands into another, because you have different "portals" into the different namespaces - the physical screen of the machine, the Drawterm connections, etc. Most errors are the result of typing the right command into the wrong namespace or machine.

I am usually around in the #plan9 irc channel as well as reachable my mail at mycroftivATsphericalharmony.com. I am always happy to assist interested users with my software.
