# Walkthrough for 9front-ANTS on Vultr from live/install cd image

### Walkthrough done with 1gb ram image size

First you will need a [vultr.com](http://www.vultr.com/?ref=6843332)(that's a referrer link) account. Next you will need to use the option to upload a custom .iso image. An uncompressed copy of the latest ANTS cd image, based on the 9front distribution, is hosted at [http://files.9gridchan.org/9frants569.iso](http://files.9gridchan.org/9frants569.iso). Create a new vps node in the datacenter closest to you, choosing to use the custom ANTS iso you uploaded from the previous url. The 1gb ram/25gb ssd size is plenty. You probably want to also check the "add private networking" box when creating it if you want to create a grid of multiple nodes.

After you boot the vps, start the control console to access the system. If the cd doesn't seem to be booting, try reselecting it from the custom iso section of the vps settings. It will prompt you for the boot cd location (accept the default), user (accept the glenda default) and graphics options (accept the defaults) and then change the screen resolution and ask for the mouse type. Resize the console window to see the whole screen. Type "ps2intellimouse" for this prompt to enable the scroll wheel. Rio will start, and you will see a small window running the "stats" command showing various sytem info, and a larger window with a prompt for the rc shell. The mouse may be hyper-responsive. You will need to click the mouse in the rc window. If you want to slow the pointer somewhat, you can type

	echo linear >/dev/mousectl

To begin the installation process, type

	inst/start

The ANTS install process is basically identical to standard 9front, as described in the [9front FQA](http://fqa.9front.org/fqa.html). It adds the option to choose to use the fossil+venti fileserver, which is a good choice for use with the Advanced Namespace Tools. If not using Fossil, I would recommend hjfs unless your vps is using a larger disk size. cwfs64x will work, but you are more likely to run out of space long-term. A sample install sequence after the inst/start command. The #entries just label which portion of the process is being described. You will mostly be accepting defaults.

	#configfs
	[enter to accept Fossil default]
	#partdisk
	sdF0
	mbr
	w
	q
	#prepdisk
	w
	q
	#mountfs
	[enter several times to accept default partitions]
	#configdist
	[enter to accept default]
	#confignet
	[enter to accept default]
	#mountdist
	[enter several times to accept default locations]
	#copydist
	[wait a few minutes while the filesystem is copied to the hard drive]
	#ndbsetup
	[choose a system name]
	#tzsetup
	[enter your timezone from the list]
	#pwsetup
	[choose a password]
	#bootsetup
	yes
	yes

After the system reboots, you will need to use the vultr website interface to remove the iso image from the machine. It will reboot again, and you will need to reopen the console to view it. Just press enter a couple times to accept the default answers to the prompts during bootup.

## using the ANTS environment ##

You probably do not want to use the vps much through the vultr console. The best method to access is with drawterm. This can be downloaded from [http://drawterm.9front.org](http://drawterm.9front.org). It is built from source for linux/bsd/os x, and there is also a windows binary available. The ANTS system listens for connections on port 17060, and you can connect as hostowner without using an auth server. A command line like
	
	drawterm -a 0.0.0.0 -h tcp!your.node.ip.address!17060 -u glenda

should give you a prompt like "glenda@9front dp9ik password:". Type the password selected, and you will then be given a cpu% rc prompt. You will begin in the minimal ANTS boot namespace. To enter a standard namespace with access to the full operating system, enter these commands:

You can start the gui by typing grio. It will just look like a grey screen. Remember to use the right mouse button menu to create new windows.

In addition to standard Plan 9, the ANTS namespace and tools are available. You can rcpu or drawterm in to the boot namespace on port 17060. To move into the main environment from there:

	rerootwin -f boot
	service=con
	. $home/lib/profile
	webfs
	grio
