# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded tgdbm 0.8 \
	    [list load [file join $dir libtcl9tgdbm0.8.so] [string totitle tgdbm]]
} else {
    package ifneeded tgdbm 0.8 \
	    [list load [file join $dir libtgdbm0.8.so] [string totitle tgdbm]]
}

package ifneeded qgdbm 0.8 \
    [list source [file join $dir qgdbm.tcl]]
