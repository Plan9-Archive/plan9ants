# Setting up two ANTS nodes to work together on Vultr
### Walkthrough done with an install created by build vultrfossil 

This walkthrough shows how to use two ants nodes together to create a small grid with automatic data replication. The starting point is completing the build vultrfossil install process described in the first vultr walkthrough.

## archive the fossil to venti, then snapshot the vm ##

	con /srv/fscons
	fsys main snap -a

This command will take a long time to complete, because the fossil is dumping all its data to the venti server. Yes, its a third copy of the basic install data! You will know when it is done because it prints a line like: vac:442c3cd34570119fcf27fc753c2130bc2225def3. ctrl-\ and then q leaves the fscons. although this may not be transmitted properly via the vultr console. You can open a different window to work in. Next we will find our current location in the venti arenas to set up the wrcmd.

	bind -b '#S' /dev #if you are working via cpu or hubfs
	fossilize /dev/sdF0/fossil
	ventiprog

At this point you are ready to snapshot the vm and clone it in vultr. This can take a little while to complete. While doing so, install some additional software and explore the system.

## configure the new secondary machine to work with the primary ##

Inside the new vm:

	cd
	cd antsexperiments/cfg
	bind -b '#S' /dev #only needed if working via cpu or hubfs
	9fs 9fat
	cp vultrplan9.ini /n/9fat/plan9.ini
	echo 'privpassword=MYPASSWORD' >>/n/9fat/plan9.ini
	
You can reboot the secondary machine to start it without fossil+venti. From the service namespace on secondary, accessed via hubfs or cpu in to port 17060:

	foshalt
	kill venti |rc
	echo reboot >/dev/reboot

Now check in vultr to learn the assigned alternate ips. The information is located in the ip4 tab of server management page. After the secondary vm reboots using hjfs again as its root fies system:

	cd antsexperiments/cfg
	startnetalt 10.99.0.11 # or whatever the machine secondary ip is
	venti/venti -c altventi.conf

Now the venti server is serving on the private ip on the /net.alt interface. Back on the primary:

	echo 'fsys main snap -a' >>/srv/fscons
	cd antsexperiments/cfg
	startnetalt 10.99.0.10

After the archival snap completes (you can check fossil/last /dev/sdF0/fossil to see the current rootscore, it should be different than the first once the snapshot finishes) then we can update the secondary venti. On the primary machine:

	ventiprog
	fossilize /dev/sdF0/fossil

Now use the vac score shown by fossilize to reset the fossil on the remote machine. on the secondary:

	venti=/net.alt/tcp!10.99.0.11!17034
	fossreset vac:74d419b929679c471cb86bc76a0872597cbf029b /dev/sdF0/fossil
	fossil/fossil -f /dev/sdF0/fossil

## explore the connected but independent namespaces ##

Now the remote fossil has been updated to be a copy of the main. You can:

	rerootwin -f fossil
	service=con
	. $home/lib/profile
	webfs

And see that changes you made since the initial setup are now present in your backup fossil.
