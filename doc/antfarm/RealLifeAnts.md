# Using ANTS for your everyday grid 
## How I use ANTS to give me the data environment I want

The tutorials show the features of ANTS but creating 5 namespaces
attached to two systems with live CDs is probably not the daily
computing environment you want.  I'll explain how I use ANTS to make a
standard Plan 9 setup work better for me.

The core of my grid is a "canonical" Plan 9 setup - Venti server,
Fossil server, tcp boot CPU server, terminal.  The venti, fossil, and
tcp cpu all run the 9pcram kernel.  The terminal is a 9front machine,
but sometimes Drawterm is used for a terminal, or another Bell Labs
system.  In addition to this main leg of the grid, a linux box runs a
p9p venti and hosts qemu vms.  The p9p venti and qemu vms duplicate
the native machines, and create an independent active copy of my main
root fs.

Using ANTS allows me to administer my venti and fileservers much more
easily, replicate data between the two independent legs of the grid,
and change my working root on the fly into any of the namespaces.  My
basic user environment is seems like any other Plan 9 userspace - as a
user, I don't have to do anything different.  However, the underlying
architecture of the namespace is created in a different way than in
standard Plan 9, and I have access to independent "service namespaces"
on each node.  If I need to reboot the venti, I don't lose the ability
to control the fossil server and tcp cpu server and use my terminal.
I can shift my active root to my other chain of machines, and then
redial the services on the native machines when the venti comes back
online.  I also have 2 remote 9 cloud nodes, one running 9pcram kernel
and Bell Labs distribution, one running 9front.  The labs node is the
the controller and runs hubfs, which links the remote nodes to the
local grid.  All interaction and control of the remote nodes is done
via the persistent hubfs.

ANTS allows me to think of "uptime" in a totally different way - not
uptime on a box, but uptime of my current active working fs.  As long
as I can keep working with my data and connecting to the namespaces I
need, rebooting a box doesn't matter.  I have had a continuous active
connection to my main root fs for the past two months, since bringing
the first version of the full ants toolkit together.  During that time
each node has been rebooted many times, usually just to upgrade to the
latest kernel version, but rerooting and multiple available root fses,
combined with no imposed chains of "reboot dependencies" between
nodes, allow me to keep working with my current data without
disruption.

## Other ways to use ANTS

You don't need a big grid to get use from these tools.  A single box
can get a lot of benefit from ANTS.  You can use the service namespace
to let you fshalt your main file server and and do things while it is
stopped.  You can set up a "be your own cpu server" environment where
you boot as a terminal, but keep an independent enviornment available
to cpu into.  One reason to do this is to use one version of Plan 9 as
your main environment, and keep an alternate distro available to cpu
into.

This is another major application of ANTS.  Per process and
independent namespaces are an easy way to work with multiple forms of
Plan 9 at once.  Some Plan 9 users have been concerned about the
growth of diverging Plan 9 distributions - ANTS offers tools which
allow multiple forms of Plan 9 to work independently side by side, but
share resources as needed.  The design of Plan 9 means that we have an
architecture which was designed from the ground up to work with
different file trees and namespaces at the same time.  ANTS is based
on Plan 9 from Bell Labs but I use other forms of Plan 9 and want to
be able to use everything at once, and combine them freely.  ANTS
helps me do this.

When installing Plan 9 to a new system, attaching to the root fs and
integrating with the network sometimes require a bit of work and
setup, and it is very useful to have a working environment available
at bootup with no dependency other than the kernel itself.  Some of
the earliest work that led to ANTS was motivated simply by reliability
and control of bootup - it is frustrating when a machine has a problem
finding a root fs and reboots with no chance to fix the problem.  With
ANTS, if there is a problem attaching to the normal root fs, you have
a working environment to debug the problem or find an alternate root.

I have tested using ANTS in a lot of ways, and I am still finding new
applications for the different pieces of the toolkit.  I am sure other
users will find many more if they explore the possibilities.
