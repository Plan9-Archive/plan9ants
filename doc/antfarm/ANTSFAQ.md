####Q: What is ANTS?

A: Software for Plan 9 from Bell Labs.  It has a customized kernel and
a package of programs and scripts to allow Plan 9 systems to do more
with namespaces - create jail-like independent namespaces, create a
"service namespace" that exists below the user namespace and
administers it, jump between namespaces with a single command, and
control the namespace of all machines on a grid with high-level
semantics.

####Q: Is ANTS a work-in-progress research project or usable, practical software?

A: It is usable, practical software for everyday use of Plan 9.  It
was created because I use a network of Plan 9 machines as my daily
computing environment, and this software lets me do the things I want
and solve the problems I have encountered.  I use and rely on this
software 24/7 to do my work and backup my data.  In my use and testing
it is robust and reliable.

####Q: Is ANTS tested on native Plan 9 hardware or just VMs?

A: I have 3 native Plan 9 machines all running the 9pcram kernel and
managed with the ANTS software.  I also use qemu vms for backup and
testing, but my primary environment is an always-on grid of native
Plan 9 systems.

####Q: Is ANTS only for grids of machines?

A: No, ANTS is useful on a single machine as well.  In fact in some
ways being able to create independent namespaces makes the biggest
difference to one-machine setups.  The dreaded "fshalt freeze" can be
banished.

####Q: ANTS seems complicated.  Isn't Plan 9 about making things simple?

A: I believe ANTS is simple at its core, even if the description can
be complicated when done in words.  Everything ANTS does is about the
core of Plan 9 - namespace operations, networking machines, using 9p
ubiquitously.  There is an inherent complexity to per process
namespaces relative to a unified namespace for every process.  Plan 9
design is based around the idea that the power and flexibility of per
process namespace is worth the cost of complexity and potential user
confusion.  ANTS is an attempt to extend the core principles of Plan 9
even further into the architecture of the system, by deliberately
creating separate and autonomous "namespace groups" that allow for
independent environments.  ANTS does not create new, competing
mechanisms to do the things that namespaces do - it uses namespaces
and extends their application in ways I believe are consistent with
the design of Plan 9.


####Q: Is Plan 9 Port supported?

A: I love p9p.  The first thing I do when setting up a unix box is
install plan9port.  One of the systems on my local grid is a p9p box.
That said, to paraphrase the 1984 VP debate: "I run servers with Plan
9.  I know Plan 9.  Plan 9 is a friend of mine.  p9p - you're no Plan
9." As I am sure the authors and contributors to p9p would be the
first to say, the absence of per process namespace as a standard in
unix makes it impossible to really replicate the system, and ANTS is
based on exactly the part of Plan 9 - per process namespace
manipulation - that is absent entirely from p9p.  That said, there are
ways that ANTS helps integrate with unix systems.  Hubfs can be used
as a replacement for non Plan9, non 9p methods of connecting and
controlling the system, and the plan9rc boot script enables Plan 9
systems to boot in new ways, including attaching more freely to
resources provided by unix systems.


####Q: Is ANTS a security system?  Can I use this to "jail" users on my systems securely?

A: Not without customization.  The design of ANTS can be understood by
comparing it to jails, but it was not created for the purpose of
isolating systems completely - instead, I use ANTS to create
independent enviornments that can share resources freely.  The sharing
resources between environments is a large part of my use and purposes,
and that is the opposite of what jails do.  In addition, the
architecture of ANTS is based on per process namespace, not on
providing separate virtualized versions of kernel devices to clients.
The bottom line is that while you could adapt the ANTS tools into a
security and isolation framework, that is not their current purpose.
By design, ANTS tries to make the namespaces independent, but very
porous to information and able to share services at will.


####Q: Is ANTS complete, or will more features be added?

A: ANTS is currently feature-complete and tested and intended for
everyday use.  It is still under "active development" so ideas for how
to extend the tools further are welcome.  My current intentions are to
work on filesystems and fileservers, for example making some of the
currently read-only fs programs able to write as well as read.  It
would be interesting to use a single tarfile as a root via a
read/write tarfs.  Other potential ideas include creating an analogous
"gui layer muxer" for rio/draw to parallel hubfs for textual
applications, and creating a GUI namespace exploration tool to
understand how the namespace of an ANTS colony of machines is
structured.
