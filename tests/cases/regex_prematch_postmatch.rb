"hello world" =~ /world/
puts $`.inspect
puts $'.inspect

"hello world" =~ /ell/
puts $`.inspect
puts $'.inspect

"no match" =~ /xyz/
puts $`.inspect
puts $'.inspect

# $` and $' update with each match
"abc def" =~ /def/
puts $`
puts $'.inspect
