# Guide to the tutorials
## What you need

The absolute minimum is a copy of qemu and the 6mb
[9worker](http://files.9gridchan.org/9worker.gz) image.  Drawterm is also
required for almost all of the tutorials.  It is also good to have
copies of the Bell Labs and/or 9front cd .iso images.  The more
advanced tutorials make use of a fully installed and modified image
that can be setup after an install from the bell labs iso, so you need
about two gigs of hdd space to make a full image if you wish to follow
those tutorials.  A copy of the fully installed 9queen can be
downloaded from
[http://sphericalharmony.com/plan9/9queen.gz](http://sphericalharmony.com/plan9/9queen.gz)

#### the first tutorial page has three walkthroughs
[tutorial](tutorial)

* the first shows using the 6mb 9worker image on its own
* the second shows using the 9worker image attached to a 9front live cd
* the third shows how to create a 9queen from a fresh install and use it to make another 9worker
* topics covered are moving between namespaces, connecting them to hubs, and modifying them via /proc

#### the next tutorial page has a single long walkthrough for two nodes
[tutorial2](tutorial2)

* uses the two images created by the previous tutorial
* demonstrates five namespaces on two nodes and free access to each

#### the third tutorial page has a single mid sized walkthrough for one node
[tutorial3](tutorial3)

* uses one node to host full Bell Labs terminal and 9front cpu server at the same time
* more practical and closer to real-world usage than earlier tutorials

#### the Setup page has setup, extra options, and troubleshooting info
[setup](setup)

* other ways of accessing the vm images
* troubleshooting potential isues
