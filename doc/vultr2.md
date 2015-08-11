# Setting up two ANTS nodes to work together on Vultr
### Walkthrough done with a build vultrfossil install

This walkthrough shows how to use two ants nodes together to create a small grid with automatic data duplication. The starting point is completing the build vultrfossil install process described in the first vultr walkthrough.

## archive the fossil to venti, then snapshot the vm ##

	con /srv/fscons
	fsys main snap -a

This command will take a long time to complete, because the fossil is dumping all its data to the venti server. Yes, its a third copy of the basic install data! You will know when it is done because it prints a line like: vac:442c3cd34570119fcf27fc753c2130bc2225def3

	fossilize /dev/sdF0/fossil
	echo 'venti/wrarena -h tcp!127.1!17034 -o 794624 /dev/sdF0/arenas 0x0' >/n/9fat/wrcmd
	ventiprog

At this point you are ready to snapshot the vm and clone it in vultr. This can take a little while to complete. In the meantime, why not do something fun like installing the 9front-ports tree and using it to install go?

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

now inside the new vm

	cd
	cd antsexperiments/cfg
	9fs 9fat
	cp vultrplan9.ini /n/9fat/plan9.ini
	echo 'privpassword=MYPASSWORD' >>/n/9fat/plan9.ini
	
on both machines, look up the ips on the vultr control panel and add them to /lib/ndb/local

	acme /lib/ndb/local
	sys=primary ip=your.ip.here

	sys=secondary ip=secondary.system.ip

The blank lines in between system entries are important. You can reboot the secondary machine to start it without fossil+venti. First on secondary in the service namespace:

	foshalt
	kill venti |rc
	echo reboot >/dev/reboot
