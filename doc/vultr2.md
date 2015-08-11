# Setting up two ANTS nodes to work together on Vultr
### Walkthrough done with a build vultrfossil install

This walkthrough shows how to use two ants nodes together to create a small grid with automatic data duplication. The starting point is completing the build vultrfossil install process described in the first vultr walkthrough.

## archive the fossil to venti, then snapshot the vm ##

	con /srv/fscons
	fsys main snap -a

This command will take a long time to complete, because the fossil is dumping all its data to the venti server. Yes, its a third copy of the basic install data! 