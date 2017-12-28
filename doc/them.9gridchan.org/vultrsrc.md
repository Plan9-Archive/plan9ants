# Walkthrough for 9front-ANTS on Vultr from source with standard 9front image

### Walkthrough done with 1gb ram image size

Create a Vultr account if you do not have one, and upload a copy of the current 9front iso. A working copy of a 9front release iso (revision 6245 from December 2017) can be found at http://files.9gridchan.org/9front6245.iso. Choose to enable a private ip. Make sure the size (smallest is good) and location are correct, and that the 9front iso is attached. Deploy!

##	Manage the vm and install through the graphical console interface ##

The general method of installation is covered in the 9front.org fqa. Hit enter a few times until the rio gui starts. You can use the live cd and begin the install with inst/start when ready. You will be choosing to use hjfs instead of the default cwfs. You have the option to do a minimal ANTS install with hjfs only, or to add fossil and venti also. To do so, we will make the hjfs partition small during the install process. Here are the relevant inputs, with everything else being left as default:

	inst/start
	hjfs #configfs
	sdF0 #partdisk
	mbr
	w
	q
	d fs #prepdisk
	a fs 204801 4400000
	w
	q
	# press enter a lot until copydist and you wait for a few minutes
	# set timezone and system name and then say
	yes # to the questions to make plan 9 mbr active
	yes

## converting base install to Advanced Namespace Tools ##

Remove the iso from the virtual machine, reboot the fresh install, and clone the ANTS repo:

	hg clone https://bitbucket.org/mycroftiv/plan9ants
	cd plan9ants

You probably want to set the rio window to 'scroll' mode. Now we will install ANTS. The command depends on whether you chose to make a small hjfs partition to allow room for fossil+venti, or will be using hjfs only. For a fossil+venti full ANTS system (requires the special partitioning during install shown above):

	build vultrfossil

If you are leaving hjfs as your sole filesystem, then instead enter:

	build vultr

Once the installs are completed (the vultrfossil script runs for a long time as it is making another full copy of the distribution into the new fossil) you can fshalt -r to reboot with ANTS installed.

## using the ANTS environment ##

In addition to standard Plan 9, the ANTS namespace and tools are available. You can rcpu or drawterm in to the boot namespace on port 17060. To move into the main environment from there:

	rerootwin -f boot
	service=con
	. $home/lib/profile
	webfs

You can also access  the early namespace by attaching hub to /srv/hubfs. It can be convenient to rimport the early namespace rcpu/exportfs listener (port 17060) from another machine also.
