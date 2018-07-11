#
# Tcl package index file
#
package ifneeded tgdbm 0.6 \
    [list load [file join $dir libtgdbm0.6.so] tgdbm]
package ifneeded qgdbm 0.6 \
    [list source [file join $dir qgdbm.tcl]]
