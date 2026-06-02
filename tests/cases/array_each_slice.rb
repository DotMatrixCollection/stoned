# each_slice splits array into chunks of n
(1..9).each_slice(3) { |s| p s }

# each_slice returns enumerator without block
p (1..7).each_slice(3).to_a

# last slice may be smaller
p (1..5).each_slice(2).to_a

# each_cons yields overlapping windows
(1..5).each_cons(3) { |c| p c }

# each_cons as enumerator
p (1..6).each_cons(2).to_a

# each_cons with window size equal to length
p [1, 2, 3].each_cons(3).to_a

# each_cons with window larger than array yields nothing
p [1, 2].each_cons(5).to_a

# each_slice on string chars
p "abcdefg".chars.each_slice(3).map(&:join)
