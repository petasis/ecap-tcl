package require Thread
puts " ++ Thread init: id=[thread::id]"
puts " > script executed: [info script] <"
flush stdout

## Load our library file...
source -encoding utf-8 \
  [file dirname [file dirname [info script]]]/ecap-tcl.tcl

::ecap-tcl::SampleHTMLProcessor create processor
