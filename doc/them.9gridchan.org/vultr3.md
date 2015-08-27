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

## another cut

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

Now the venti server is serving on the private ip on the /net.alt interface.

## snippets

Next we will find our current location in the venti arenas to set up the wrcmd.

	cd antsexperiments/cfg
	startnetalt 10.99.0.10

	venti=/net.alt/tcp!10.99.0.11!17034

