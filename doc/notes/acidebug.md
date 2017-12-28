#Acid debugging basics

### How to step through a program and inspect data structures

To help acid print data structures, compile with the -a or -aa options:

	8c -FTVwaa hubfs.c >$home/hubfs.acid

After a hubfs is started, attach acid to it:

	acid -l $home/hubfs.acid hubfspid

Check the current status of the execution stack and variables:

	lstk()

Set a breakpoint:

	bpset(fswrite)

Acid says "waiting". Cause a write to the fs in the system outside of acid, then in acid:

	cont()

Now we have hit the breakpoint, check stack and step through a few lines of source:
	
	lstk()
	next()
	next()
	next()

Check the current stack and variables again, note that h has been set:

	lstk()

Use the address shown of h to print the data of the current Hub:

	Hub(0xe1ce0)

Delete the breakpoint

	bpdel(fswrite)

Allow the program to continue freely:

	cont()

Hit delete key to interrupt waiting, ctrl-D to exit acid.
