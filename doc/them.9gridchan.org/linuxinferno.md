#Walkthrough for setting up a linux+inferno node to interface with ANTS

### Assumes a linux or other unix system with plan9port and inferno

Linux and unix can do p9sk1 authentication to plan 9 systems, but unfortunately does not yet support the encryption option of import. So, if we wish to have a truly secure control channel across an untrusted network, we use hosted inferno to provide the channel. Note that Inferno does not yet support the newer 9front rcpu/rimport mechanisms, so this assumes that you are running old-fashioned exportfs listeners.

## Setup auth and network database

For simplicity of administration, we are going to patch our Inferno system to let us user p9sk1 (plan 9 style) authentication + encryption. The patch for this is located at patched/patchinfp9sk1. 

	cp ants2.1/patched/patchinfp9sk1 inferno/patchinfp9sk1
	cd inferno
	patch -p1 <patchinfp9sk1
	cd appl
	mk install

Now we need to set up an auth server for inferno to talk to. Many of the other pages walkthroughs use plan 9 factotum to factotum authentication, which works without an auth server as long as there is a shared key. There are standard guides to setting up an auth server in the 9front FQA and bell labs wiki. This guide assumes that you will set up on the authserver on the Plan 9 system acting as the hubfs server. The simplest way is to issue these commands:

	auth/keyfs
	auth/changeuser glenda
	aux/listen1 -t -v tcp!*!567 /bin/auth/authsrv &

You may wish to verify your auth server is working correctly. Now, edit the file inferno/lib/ndb/local to tell it how to authdial your server. Add a blank line and lines in the following format:
	
	sys=hubservername ip=ip.of.hubserver
		dom=keydomain auth=hubservername

## Controlling linux nodes with hubfs

We will have the linux host start emu inside screen, and the emu will dial a hubfs server and create a secure connection, mount the hubfs, create hubfiles, and then start a loopback listener for the linux host to use:

	emu
	ndb/cs
	auth/factotum
	echo 'key proto=p9sk1 dom=yourdom user=glenda !password=yourpass' >/mnt/factotum/ctl
	import -e 'rc4_256 sha1' -c server.address / /n/remote
	mount -A -c /n/remote/srv/gridhub /n/gridfs
	cd /n/gridfs
	touch inf0 inf1 inf2 lin0 lin1 lin2
	sh <inf0 >>inf1 >>[2]inf2 &
	listen -Av tcp!127.0.0.1!9999 {export /n/gridfs&}
	cd

Detach from your screen and on the linux host

	mkdir hubmnt
	9pfuse 'tcp!127.0.0.1!9999' hubmnt
	bash <hubmnt/lin0 >>hubmnt/lin1 2>>hubmnt/lin2 &

Now you can control the linux node and its hosted inferno from any other node on your grid:

	import -E ssl -c hubserver.address / /n/hubserver
	mount -c /n/hubserver/srv/gridhub /n/hubfs
	hub
	%attach lin

And you are controlling the bash shell on the linux host attached to the hubfs. Note that you have no bash prompt!

## Exporting your linux host filesystem to other grid nodes

Assuming emu is started and the factotum holds the appropriate p9sk1 key, emu can bind the host filesystem and then start an authenticated and encrypted listener to share it with the plan 9 systems on your grid:

	bind -c '#U*' /n/local
	listen -Av tcp!*!19999 {9export -e 'rc4_256 sha1'&}

Alternatively, you may wish to announce this service to the registry with grid/reglisten. Plan 9 clients can connect as follows:

	import -E ssl -c tcp!linux.serveraddress!19999 / /n/remoteunix

Another option that bypasses inferno is to use u9fs inside an ssh tunnel. If you are not concerned with encryption but only authentication, plan9port factotum can do the authentication when connecting to hubfs and u9fs can do p9sk1 authentication of clients. Given the current state of public networking, encryption is recommended when used on any non-secured network. 
