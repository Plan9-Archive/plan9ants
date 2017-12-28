# Setting up two ANTS nodes to work together on Vultr

### Walkthrough done with an install created by build vultrfossil 

This walkthrough shows how to use two ants nodes together to create a small grid with automatic data replication. The starting point is completing the build vultrfossil install process described in the first vultr walkthrough.

## archive the fossil to venti, then snapshot the vm ##

	con /srv/fscons
	fsys main snap -a

This command will take a long time to complete, because the fossil is dumping all its data to the venti server. Yes, its a third copy of the basic install data! You will know when it is done because it prints a line like: vac:442c3cd34570119fcf27fc753c2130bc2225def3. ctrl-\ and then q leaves the fscons. although this may not be transmitted properly via the vultr console. You can open a different window to work in. 

	bind -b '#S' /dev #if you are working via rcpu or hubfs
	fossilize /dev/sdF0/fossil

At this point you are ready to snapshot the vm and clone it in vultr. This can take a little while to complete. While doing so, install some additional software and explore the system.

## configure the new secondary machine to work with the primary ##

Back on the primary, make sure you have added some data to the filesystem since you made the clone of the vm via the vultr management interface. Then initiate another snapshot:

	echo 'fsys main snap -a' >>/srv/fscons

After the archival snap completes (you can check fossil/last /dev/sdF0/fossil to see the current rootscore, it should be different than the first once the snapshot finishes) then we can update the secondary venti. On the primary machine:

	rimport -c tcp!secondary.ip!17060 /net /net.alt
	9fs 9fat
	ventiprog
	fossilize /dev/sdF0/fossil

Now use the vac score shown by fossilize to reset the fossil on the remote machine. Access the secondary machine service namespace via hub or rcpu:

	fosreset vac:74d419b929679c471cb86bc76a0872597cbf029b /dev/sdF0/fossil

## verify the updated fossil includes new data ##

Now the remote fossil has been updated to be a copy of the main. You can just reboot the machine, or alternatively, from the service namespace:

	fossil -f /dev/sdF0/fossil
	rerootwin -f fossil
	service=con
	. $home/lib/profile
	webfs

And see that changes you made since the initial setup are now present in your backup fossil.
