# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded tgdbm 0.7 \
	    [list load [file join $dir libtcl9tgdbm0.7.so] [string totitle tgdbm]]
} else {
    package ifneeded tgdbm 0.7 \
	    [list load [file join $dir libtgdbm0.7.so] [string totitle tgdbm]]
}

package ifneeded qgdbm 0.7 \
    [list source [file join $dir qgdbm.tcl]]
