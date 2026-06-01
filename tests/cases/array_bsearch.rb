arr = [1, 3, 5, 7, 9, 11]
puts arr.bsearch { |x| x >= 5 }
puts arr.bsearch { |x| x >= 6 }
puts arr.bsearch { |x| x >= 100 }.inspect
puts arr.bsearch_index { |x| x >= 5 }
puts arr.bsearch_index { |x| x >= 100 }.inspect
puts [].bsearch { |x| x >= 1 }.inspect
puts (1..20).to_a.bsearch { |x| x >= 15 }
puts (1..20).to_a.bsearch_index { |x| x >= 15 }
puts arr.bsearch { |x| 7 - x }
puts arr.bsearch_index { |x| 7 - x }
puts arr.bsearch { |x| 8 - x }.inspect
puts arr.bsearch.class
begin
  arr.bsearch { |x| "bad" }
rescue TypeError => e
  puts e.class
end
