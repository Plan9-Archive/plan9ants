# Additional configuration and usage of ANTS nodes on vultr
### Walkthrough done with two nodes configured with replicted venti+fossil

On both machines, look up the ips on the vultr control panel and add them to /lib/ndb/local:

	acme /lib/ndb/local
	sys=primary ip=your.ip.here

	sys=secondary ip=secondary.system.ip

The blank lines in between system entries are important. 
