# Walkthrough for hubfs-controlled boot and announcing to Inferno registry
### Works with any ANTS installs + optional Inferno

You need a grid server which will serve hubfs via standard cpu export and optionall inferno with the inferno registry.

In plan9.ini

	rootstart=grid
	hubserver=name or dialstring

Will use /rc/bin/hubrc instead of the usual cpurc or termrc. The node will boot and connect to the hubserver, which needs to prove a /srv/gridhub for cllients to connect to. 