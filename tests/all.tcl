# -*- tcl -*-
# Run all of the Tcl tests in this directory (use the Tcltest-package)

if {[lsearch [namespace children] ::tcltest] == -1} {
	package require tcltest
	namespace import ::tcltest::*
}

foreach file {tgdbm.test qgdbm.test} {
    puts "testing: $file"
    source $file
}
::tcltest::cleanupTests
