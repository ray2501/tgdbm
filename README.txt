This is a Tcl-Wrapper for the famous gdbm (the GNU-Version of dbm) and
a small database-utility "Qgdbm":

Tgdbm and Qgdbm Version 0.5 (December 12, 2003 / April 14, 2005)
                             Stefan Vogel (stefan  at vogel-nest dot de)

The   Tgdbm-package  makes the GDBM-Functions available inside of Tcl. 

With  calling the  Tcl-function  "gdbm_open" a  handle  to the  opened
gdbm-file  is  returned.  The  retrieved  handle  can  be  used  as  a
Tcl-command for further access to the opened file.
(e.g.: 
% set gdbm [gdbm_open -wrcreat myfile.gdbm]
% $gdbm count
)

Furthermore you find Qgdbm in this distribution, which works on top of
the  Tgdbm-library.  Qgdbm,  which  is  the  shortcut  for  Query-Gdbm
provides a set of routines for simple SQL-like queries. These SQL-like
queries are adapted to the usual Tcl-style. By redefining SQL in terms
of Tcl-List not  only the syntax is  more easy but it is  easier to be
included in Tcl-applications.

For documentation see the documentation on
http://www.vogel-nest.de -> Tgdbm/Qgdbm

---------------------------------------------------------------------------
BUILD Tgdbm:

o Unix:
  You should have installed the latest gdbm-version (1.8.3) and one of
  the latest  tcl-version (8.4.4) on  your machine. GDBM can  be found
  at: http://www.gnu.org/software/gdbm/gdbm.html Tcl  can be found at:
  http://www.tcl.tk
  Simply compile  the file "tgdbm.c"  and build a shared  library. You
  can use  the included Makefile  as a starting-point. It's  so simple
  that I didn't  wanted to provide a configure-script,  ... (hey, it's
  only one file)
  Take  the  Makefile-example  and  simply  adapt  your  include-  and
  library-Paths for Tcl and gdbm.
  A simple Makefile (named Makefile) is included. It assumes tcl.h to
  reside in /usr/include/tcl8.4 and the tcl-library named tclstub8.4.
  Check these paths.
  A dev-paket of tcl has to be installed on the system.
  A pre-build shared-library build on ubuntu 7.10 Gutsy Gibbon with
  Tcl8.4 and Gdbm 1.8.3 is included in the distribution.

o Windows:
  This  assumes you  have installed  the  latests windows-gdbm-version
  (1.8.3) and one of the latest  tcl-version on your machine. Gdbm for
  windows can be found at http://www.vogel-nest.de -> Gdbm

  You could take  the gdbm.dll and gdbm.lib from  that distribution or
  adapt the Makefile's to your needs and build it on your own.

  Visual-C++
  If you have the VC++-Compiler look at the first lines of Makefile.vc
  and adapt the Paths for VC++, Gdbm and Tcl. Then

  > nmake -f Makefile.vc
  (Or rename Makefile.vc to Makefile and simply
  > nmake
  )

  Mingw32
  For the most people, who  do not have a Visual-C++-Compiler or don't
  like   it  (e.g.   me),  I   have  provided   a  Makefile   for  the
  Mingw32-compiler and  make utilities. I assume you  have the regular
  utilities installed too (e.g. rm, mkdir, mv).

  Simply open a shell "sh" and type:
  > make -f Makefile.mingw
  or copy Makefile.mingw to Makefile and
  > make
  Several warnings occur, but they are harmless (at least I hope so)

  Cygwin
  I didn't spent  the time to build a  Cygnus-Makefile, happily I received
  the makefile from Alejandro Lopez-Valencia.

---------------------------------------------------------------------------
TEST Tgdbm and Qgdbm:

To test the  distribution you can call (on unix)  either: make test Or
do the tests manually. Start the tcl-shell in the directory "tests" of
this distribution  and source the  all-script. It is assumed  that you
have  the  gdbm-library installed  somewhere  in  your  paths on  your
system.
> cd tests
> tclsh all.tcl

If some tests fail please tell me.

---------------------------------------------------------------------------
INSTALL Tgdbm and Qgdbm:

You can simply use the Makefile (on Unix) and call: make install_pkg
On windows call:
> nmake -f Makefile.vc install_pkg
if you have VC++ installed or
> make -f Makefile.mingw install_pkg
for Mingw.

Or you do it manually.
Simply start a tcl-shell in this directory with the install-script:
% tclsh install.tcl

This will create a subdirectory "tgdbm0.5" in the tcl-distribution and
copy the library tgdbm and the tcl-scripts: qgdbm.tcl and pkgIndex.tcl
to this directory.
On Unix you should call this as a user who has permissions to create a
directory in the tcl install-directory.

It is assumed that the  GDBM-Library (that is: gdbm.dll or gdbm.so) is
accessible in your  PATH. So this is either  in the standard-directory
/usr/local/lib on Unix. 
Or you can  copy the provided gdbm.dll to  your system-path on Windows
(e.g. C:/WINNT/system32 or look at WINDIR).

If you  want to install Tgdbm/Qgdbm completely  yourself simply create
the  directory  "tgdbm0.5"  under  <tcl-directory>/lib/ and  copy  the
following files to that directory:
 * pkgIndex.tcl
 * tgdbm.dll/so

If  you don't  have gdbm  installed on  your system  you can  copy (on
windows) gdbm.dll to <tcl-directory>/bin/


---------------------------------------------------------------------------
The following files are included in this distribution (0.5):

tgdbm.c              - Tcl-Wrapper for Gdbm
tgdbm.dll            - Prebuild tgdbm-Library for Windows (build with VC++ 6.0
                       and gdbm-1.8.3 and Tcl8.4 on Windows2000)

Makefile             - Simple Makefile for Unix-Systems (should be adapted to
                       personal needs)
Makefile.mingw       - Makefile for Windows to be used with Mingw32.
                       This assumes you have a Tcl83.lib-File compiled with
                       Mingw32
Makefile.vc          - A Visual-C++ Makefile which could be used with tcl83.lib
                       distributed with Tcl.

qgdbm.tcl            - Tcl-Package based on Tgdbm for simple database-queries
README.txt           - This file
CHANGES.txt          - A list of changes from last releases
install.tcl          - Tcl-Script for installation, simply source it into 
                       your tcl-shell (the tcl-shell should be opened in this
                       directory)
tests/all.tcl        - runs all test-scripts (uses Tcltest-package Tcl8.3)
tests/tgdbm.test     - Testcases for Tgdbm
tests/qgdbm.test     - Testcases for Qgdbm
tests/demo.tcl       - a very simple gdbm-viewer which stores it's
                       settings in a "option.gdbm"-file (like an
					   ini-file). Needs the tablelist-widget from
					   http://www.nemethi.de
