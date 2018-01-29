#
# Tcl package index file
#
package ifneeded tgdbm 0.5 \
    [list load [file join $dir libtgdbm0.5.so] tgdbm]
package ifneeded qgdbm 0.5 \
    [list source [file join $dir qgdbm.tcl]]
