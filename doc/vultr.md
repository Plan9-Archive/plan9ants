# Walkthrough for installing ANTS-9front on Vultr
### Walkthrough done with smallest image size
#### Thanks to Vultr for their excellent service

	CREATE a Vultr account if you do not have one
	UPLOAD a copy of the current 9front iso. 

A working copy of a 9front iso can be found at http://9gridchan.org/9front.iso. Choose to enable a private ip. Make sure the size (smallest is good) and location are correct, and that the 9front iso is attached. Deploy!

	INSTALL through the console management interface on vultr

Mostly you will be using the defaults. The general method of installation is covered well in the 9front.org fqa. You will be choosing to use hjfs instead of the default cwfs, and we recommend partitioning the hjfs partittion to be small.

	hjfs
	sdF0
	mbr
	w #partdisk
	q
	d fs #prepdisk
	a fs 204801 2200000
	w
	q
	32 # mb cache
	# enter a lot until copydist and you wait for a few minutes
	# set timezone and system name and then say
	yes yes # to the questions to make plan 9 mbr active

## converting base install to Advanced Namespace Tools ##

	REMOVE the iso from the virtual machine and restart
	HG CLONE https://bitbucket.org/mycroftiv/antsexperiments

Reboot the fresh install, and clone the ANTS repo. You probably want to set the rio window to 'scroll' mode. 

	cd antsexperiments
	build 9front
	build 9frontinstall
	9fs 9fat
	acme /n/9fat/plan9.ini

You will need to set several values here. The most important is a new privpassword and changing the bootargs to use hjfs.

	bootargs=local!/dev/sdF0/fs -m 32
	privpassword=yourchoice
	fatpath=/dev/sdF0/9fat
	bootcmd=plan9rc
	bootfile=9ants

Put your changes to save them, and then fshalt -r to reboot as an ANTS node. Bootup will proceed normally, with an ANTS independent namespace underneath the usual user namespace. Drawterm or cpu in on port 17060, or access the /srv/hubfs mounting it to /n/hubfs and running the hub script.

## adding fossil and venti ##

Now we will expand our capabilities with archival storage and fast filesystem replication. 

	disk/prep /dev/sdF0/plan9
	a fossil 2200000 10000000
	a arenas 10000000 30000000
	a isect 30000000 31455207
	w
	q

