# Walkthrough for setting up Inferno as a registry server on Plan 9

### Works for any Plan 9 install

Inferno provides a dynamic registry service for grid nodes to communicate what services they are offering. We will install Inferno to plan 9 and start up registry service. Start by fetching a recent tarball of the distribution, then edit the mkconfig, then mk install as follows:

	hget http://www.vitanuova.com/dist/4e/inferno-20150328.tgz >inferno-20150328.tgz
	tar xzf inferno-20150328.tgz
	cd inferno
	ed mkconfig
	7c
	ROOT=/usr/glenda/inferno
	.
	17c
	SYSHOST=Plan9
	.
	w
	q
	path=`{echo $path /usr/glenda/inferno/Plan9/386/bin}	
	mk nuke
	mk install

Inferno should be built now. You can test with

	emu

To start inferno registry service, we can execute the following minimal set of commands inside emu:

	bind -c '#U*/net' /net
	mount -A -c {ndb/registry} /mnt/registry
	listen -A tcp!*!6675 {export /mnt/registry&}

This is a public, non authenticated registry. Inferno can announce services with grid/reglisten and Plan 9 can use the gridlisten utility available in ANTS/patched.

## Todo: describe the signer key process, using plan 9 srv, plan 9 auth, hubfs ##

	bind -c '#₪' /srv
	ndb/cs
	svc/net
