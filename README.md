tgdbm
=====

It is a clone for [Tgdbm package](http://www.vogel-nest.de/tgdbmqgdbm-library-for-tcl-version-0-5/).
I use TEA to build this package.

Only test on openSUSE LEAP 42.3.
If you want to buld this package, it is necesarry to install development files for GDBM:

	sudo zypper in gdbm-devel

And I update the error code table since GDBM_UNKNOWN_UPDATE is not used in my current GDBM version.
