# An Overview of the Advanced Namespace Tools and the ANTS farm
## Next-level use and control of Plan 9 from Bell Labs namespaces

The purpose of the antfarm sub-site is to provide demonstrations,
tutorials, and explanations of the Advanced Namespace ToolS.  There
are many new things in ANTS and it isn't easy to understand how
everything fits together without seeing it in action and using the
system.  These pages are based around the small
[9worker](http://9gridchan.org/9worker.gz) vm image for Qemu, and a
matching 9queen full install that can be generated with a script from
a clean install from the Bell Labs .iso.

## What ANTS has, is, and does

* A new bootup process that keeps the main flow of control independent of a root fs
* Boot scripts and tools to give you precise control of boot but keep backwards compatibility
* Creates a new service namespace beneath normal userspace for administration
* Allows multiple independent namespaces to run on the same machine 
* Move between namespaces on demand with rerootwin and other scripts
* Modify namespace of other processes via /proc, including those on remote machines
* Create persistent shells in any namespace that can be accessed from any namespace
* Use the same foundation (hubfs) for direct free piping of data between nodes 
* Integrate persistent access to all namespaces into the gui and provide information about namespace location
* Automate progressive replication of archival data to multiple archive locations
* Automate creating creating independent backup copies of the current active root fs
* Remove any sequential dependencies in machine boot order and forced reboots due to sources other than kernel level failure
* Maintains the normal Plan 9 userland without disruption or change to existing software 
* Create a usable micro-distribution of Plan 9 similar in concept to busybox.

## Purpose of the vm image and tutorials

The main purpose is to demonstrate the features of ANTS and how they
can be used.  The 9worker image is also independently useful as a
micro-cpu server that delivers a lot of Plan 9 in a tiny package.  The
tutorials are not intended to show how ANTS is used in everyday use,
but to illustrate the principles of creating and moving between
namespaces and modifying them.  In everyday use, I mostly use Plan 9
normally, and the underlying architecture doesn't affect this.  The
main notable difference in my user environment is my access to
persistent shells on all machines via hubfs, but the hubfs software is
independent of the other tools and modifications, although it is
synergistic with them.  However, the ANTS infrastructure is vital for
how I keep my data backed up, how I administer my machines, how I cope
with any technical difficulties that occur with hardware or the
network, and how I add or remove machines from my grid.  It also helps
me use both Bell Labs and 9front versions of Plan 9 at the same time
without conflicts.

Hopefully, the vm-based demonstrations will make the purpose and use
of the tools clear enough that others will be able to receive benefit
from them as well.  In addition to these tutorials and pages, there is
also extensive documentation and a full-length paper contained within
the ants directory at
[http://ants.9gridchan.org](http://ants.9gridchan.org)
