# ANTS/9front Live+Install CD .iso image

### ANTS r551 9front r6274 released Dec 27 2017

## [http://files.9gridchan.org/9frants555.iso.gz](http://files.9gridchan.org/9frants555.iso.gz)

The Advanced Namespace Tools for Plan 9 are now available for testing and installation as a custom spin of the 9front live/install cd image. The cd boots the 9ants custom kernel and includes all userspace tools, and can installs the full ANTS system. Installation is the same as standard 9front, the command inst/start beings the process. You can experiment with most of the new features without needing to install.

### New features and applications

#### Kernel

* The namespace of processes can be manipulated via the /proc/pid/ns file.

To change the namespace of a process you own

	echo 'bind /foo /bar' >/proc/pid/ns

where "pid" is the process id. Binds, mounts, unmounts and the standard flags are supported. Mounts requiring a new authentication are not supported.

* Private /srv namespaces are available.

The #s kernel device is handled in the manner of /env. In rc,

	rfork V

will move you into a new, clean /srv. Your existing file descriptors are unaffected, so your existing mounts (such as that of the root fileserver) continue to function. New child processes will inherit this /srv. A global srv which is not forked is available to all processes at #z. The #z device is not bound anywhere in the standard namespace but works analogously to original /srv once it is.

#### Applications: hubfs i/o muxing persistence and grio customized rio

* Hubfs allows persistent shared textual applications similar to tmux/screen
* Can also be used as a general purpose multiclient message queue via 9p

Hubfs provides general purpose persistent pipelike buffered files as a 9p filesystem. It is usually used to provide shared access to instances of the rc shell and applications like ircrc. To create a new instance or attach to an existing one, a wrapper script is used like this:

	hub FSNAME

Multiple clients can attach to the same fs, each able to write to it, with reads being sent to all of them. For details on usage, see man hubfs.

* Grio customized rio integrates with hubfs and offers color selection and a customizable command in the menu

The standard rio is launched by the default profile, but the "grio" command will create subrios using the customized ANTS grio. It offers several new features. It adds a "hub" command to the menu, which connects to whatever instance of hubfs is mounted at /n/hubfs. If none is mounted, it creates a new one. It adds the ability to add a custom command of your choice to the menu, by default /bin/acme. -x command -a flags/argument sets the custom command. The argument of -a cannot include spaces. Fully customizable colors are available and specified via their hex values. Check /sys/src/9/ants/frontmods/grio/README for full information. Sample command to start a light blue grio with stats -lems in the menu:

	grio-c 0x49ddddff -x /bin/stats -a -lems

#### Independent boot/admin namespace and namespace manipulation scripts

* ANTS boot process creates a separate namespace with no connection to the root fileserver

The modified ANTS boot sequence creates a self-sufficient admin environment using a ramdisk and the kernel's compiled-in paqfs. A full ANTS install to disk also includes additional utilities loaded into the ramdisk from a tools.tgz file stored in the 9fat partition. On the livecd, you can access this namespace either via the hubfs at /srv/hubfs started during boot, or by providing a password to the rcpu listener on port 17060 also started during boot. A standard key-adding command like:

	echo 'key proto=dp9ik user=glenda dom=antslive !password=whatever' >/mnt/factotum/ctl

enables remote access to the independent namespace via drawterm/cpu to port 17060.

* Namespace manipulation scripts such as rerootwin

Several scripts are provided to assist in working with multiple namespaces. The most important is rerootwin, which is somewhat analogous to unix 'chroot' and maintains connection to the input and draw devices. If you drawterm/cpu into the independent boot/admin namespace, you will probably wish to shift into the standard namespace with all applications and services available. To do this, use the following sequence within the admin namespace:

	rerootwin -f boot
	service=con
	. $home/lib/profile
	grio #optional

You now have an environment that behaves the same as the main environment, although if you check your ns, you will see it is constructed rather differently. Other namespace manipulation scripts include "cpns SRC DEST", which uses the writable ns in /proc to attempt to modify the namespace of process DEST to match that of process SRC. This is done with a rather crude textual comparison of the contents of the ns files, so results in may practice may be somewhat unpredictable. See man rerootwin and man cpns for more information.

### Installation

* Install process is the same as for standard 9front with addition of fossil+venti option

ANTS is compatible with the standard 9front fileservers, but restores the option of installing fossil and venti because fossil rootscores offer a powerful mechanism for efficiently working with multiple root filesystems. Fossil is generally considered less reliable than the other fileservers, so if you do choose to use Fossil, make sure to have a good backup system for your data. ANTS includes some tools for assisting with replicating data between Venti servers and managing rootscore archives. See man ventiprog for usage example.

* Install process lets you add a password to plan9.ini to setup boot/admin namespace access

The only additional step in the installer, pwsetup, just adds a value to plan9.ini to provide a password for access to the independent admin namespace. Note that it does not set up a full authsrv/keyfs system, it just adds the password to factotum for hostowner access on port 17060.

* Install makes plan9rc the default booting command/method

The live cd uses an ANTS-customized bootrc, but the full install sets the ANTS plan9rc boot script This script is backward compatible with the standard boot process, but offers several new options and possibilities, including the ability to hook custom commands during the boot process, root to cpu servers with aan for reliability, or even connect to a remote hubfs to control the boot process and announce services to an inferno registry. See man plan9rc for some details, although not all possibilities are currently documented.

### Who is this for?

ANTS should be regarded as experimental software intended for experienced Plan 9 users. It should behave for the user identically to standard 9front but newer users are recommended to use the standard 9front distribution found at [http://9front.org](http://9front.org). The kernel modifications and nonstandard boot process/environment work well in practice, but more testing is required to guarantee that they do not introduce any instabiity and/or security risks. ANTS is designed to assist in using Plan 9 as a true distributed system with multiple nodes. It works fine on a single, local box, but offers the most benefits when used with multiple systems in accordance with the original Plan 9 distributed design.

#### Credits and Thanks

The vast majority of code on the live/install cd is the same as standard 9front, which builds on the earlier work of Bell Labs. The intention of this release is to offer an easy to test and install ANTS environment for 9front developers and users who are curious about these namespace tools. It is not intended as a new fork or competitor to standard 9front. Thanks especially to Cinap Lenrek for his leadership of the 9front project and generous time and assistance with everything I have needed to learn during the course of ANTS development.

#### Known issues

* The ANTS source code at /sys/src/ants is missing the hubfs1.1 directory. Perhaps the cdproto file that generates the .iso filters it out for some reason.
* Updating and rebuilding the system using the 9front sysupdate command may result in the loss of some ANTS features, and require rebuilding/reinstalling some of the ANTS toolkit, because ANTS attempts to mostly contain its modifications and not overwrite the standard distribution, so for instance the customized rc with rfork V available will be overwritten if the system is rebuilt with a standard mk install in /sys/src. 
* Some documentation is out of date and documentation is spread out between manpages and multiple places on the website.
