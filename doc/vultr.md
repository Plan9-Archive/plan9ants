# Walkthrough for installing ANTS-9front on Vultr
### Walkthrough done with smallest image size
#### Thanks to Vultr for their excellent service

	CREATE a Vultr account if you do not have one
	UPLOAD a copy of the current 9front iso. 

A working copy of a 9front iso can be found at http://9gridchan.org/9front.iso. Choose to enable a private ip. Make sure the size (smallest is good) and location are correct, and that the 9front iso is attached. Deploy!

	MANAGE the vm, start if needed, and view the console
	INSTALL through the console management interface on vultr

Mostly you will be using the defaults. The general method of installation is covered well in the 9front.org fqa. You hit enter a few times until the rio gui is started, then you can use the live cd and begin the install with inst/start when ready. You will be choosing to use hjfs instead of the default cwfs. You have the option to do a minimal ants install with hjfs only, or to add fossil and venti also. To do so, we will make the hjfs partition small during the install process. Here are the relevant inputs, with everything else being left as default.

	inst/start
	hjfs #configfs
	sdF0 #partdisk
	mbr
	w
	q
	d fs #prepdisk
	a fs 204801 2200000
	w
	q
	# enter a lot until copydist and you wait for a few minutes
	# set timezone and system name and then say
	yes yes # to the questions to make plan 9 mbr active

## converting base install to Advanced Namespace Tools ##

	REMOVE the iso from the virtual machine and restart
	hg clone https://bitbucket.org/mycroftiv/antsexperiments
	cd antsexperiments

Reboot the fresh install, and clone the ANTS repo. You probably want to set the rio window to 'scroll' mode. Now we will install ANTS. The command depends on whether you chose to make a small hjfs partition to allow room for fossil+venti, or will be using hjfs only. For a fossil+venti full ANTS system:

	build vultrfossil

If you are leaving hjfs as your sole filesystem, then instead enter

	build vultr

## old stuff before i wrote the custom vultr scripts

	build 9front
	build 9frontinstall
	9fs 9fat
	cp cfg/vultrplan9.ini /n/9fat/plan9.ini
	acme /n/9fat/plan9.ini

You will need to set several values here. The most important is a new privpassword and changing the bootargs to use hjfs.

	bootargs=local!/dev/sdF0/fs -m 32
	privpassword=yourchoice
	fatpath=/dev/sdF0/9fat
	bootcmd=plan9rc
	bootfile=9ants
## already in vultrplan9.ini

Put your changes to save them, and then fshalt -r to reboot as an ANTS node. Bootup will proceed normally, with an ANTS independent namespace underneath the usual user namespace. Drawterm or cpu in on port 17060, or access the /srv/hubfs mounting it to /n/hubfs and running the hub script.

## adding fossil and venti ##

Now we will expand our capabilities with archival storage and fast filesystem replication. 

	disk/prep /dev/sdF0/plan9
	a fossil 2200000 10000000
	a arenas 10000000 30000000
	a isect 30000000 31455207
	w
	q

