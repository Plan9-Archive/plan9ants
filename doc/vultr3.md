# Additional configuration and usage of ANTS nodes on vultr
### Walkthrough done with two nodes configured with replicted venti+fossil

On both machines, look up the ips on the vultr control panel and add them to /lib/ndb/local:

	acme /lib/ndb/local
	sys=primary ip=your.ip.here

	sys=secondary ip=secondary.system.ip

The blank lines in between system entries are important. 

In the meantime, why not do something fun like installing the 9front-ports tree and using it to install go?

	[optional]
	cd
	hg clone https://bitbucket.org/mveety/9front-ports
	mkdir /sys/ports
	bind -c 9front-ports /sys/ports
	cd /sys/ports
	touch Config/ports.db
	cd dev-lang/go
	webfs
	mk all
	bind -b $home/9front-ports/dev-lang/go/work/go/bin /bin
	go run work/go/test/helloworld.go

## cut from vultr2 while simplifying

	9fs 9fat
	acme /n/9fat/wrcmd

Change the wrcmd to target the other venti by changing it to use the /net.alt interface and the secondary server ip. example:

	venti/wrarena -h /net.alt/tcp!10.99.0.11!17034 -o 794624 /dev/sdF0/arenas 0x107f510b
