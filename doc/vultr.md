# Walkthrough for installing 9front-ANTS on Vultr
### Walkthrough done with smallest image size
#### Thanks to Vultr for their excellent service

	CREATE a Vultr account if you do not have one
	UPLOAD a copy of the current 9front iso. 

A working copy of a 9front iso can be found at http://9gridchan.org/9front.iso. Choose to enable a private ip. Make sure the size (smallest is good) and location are correct, and that the 9front iso is attached. Deploy!

##	Manage the vm and install through the graphical console interface ##

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

Remove the iso from the virtual machine and restart

	hg clone https://bitbucket.org/mycroftiv/antsexperiments
	cd antsexperiments

Reboot the fresh install, and clone the ANTS repo. You probably want to set the rio window to 'scroll' mode. Now we will install ANTS. The command depends on whether you chose to make a small hjfs partition to allow room for fossil+venti, or will be using hjfs only. For a fossil+venti full ANTS system:

	build vultrfossil

If you are leaving hjfs as your sole filesystem, then instead enter

	build vultr

Once the installs are completed (the vultrfossil script runs for a long time as it is making another full copy of the distribution into the new fossil) you can fshalt -r to reboot with ANTS installed

## using the ANTS environment ##

In addition to standard Plan 9, the ants namespace and tools are available. You can cpu or drawterm in to the boot namespace on port 17060. To move into the main environment from there,

	rerootwin -f boot
	service=con
	. $home/lib/profile
	webfs

You can also access  the early namespace by attaching hub to /srv/hubfs. It can be convenient to import the early namespace exportfs listener (port 17027) from another machine also.
