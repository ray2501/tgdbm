tgdbm
=====

It is a clone for [Tgdbm package](http://www.vogel-nest.de/tgdbmqgdbm-library-for-tcl-version-0-5/).
I use TEA to build this package.

Only test on openSUSE LEAP 42.3 and Ubuntu 14.04.
If you want to build this package on openSUSE, it is necesarry to install development files for GDBM:

	sudo zypper in gdbm-devel

If you want to build this package on Ubuntu, it is necesarry to install development files for GDBM:

	sudo apt-get install libgdbm-dev

And I update the error code table since GDBM_UNKNOWN_UPDATE is not used in my current GDBM version.
