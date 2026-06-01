puts (1..100).bsearch { |n| n >= 42 }
puts (1...100).bsearch { |n| n >= 99 }.inspect
puts (1...100).bsearch { |n| n >= 100 }.inspect
puts (10..1).bsearch { |n| n >= 5 }.inspect
puts (1..100).bsearch { |n| 42 - n }
puts (1..100).bsearch { |n| 101 - n }.inspect
puts (1..3).bsearch.class
puts (1..3).respond_to?(:bsearch)
puts (0.0..10.0).bsearch { |n| n >= 3.5 }.inspect
puts (0.0..10.0).bsearch { |n| 3.5 - n }.inspect
puts (0.0...10.0).bsearch { |n| n >= 10.0 }.inspect
puts (0.0..1.0).bsearch { |n| n >= 0.25 }.inspect
begin
  ("a".."z").bsearch { |s| s >= "m" }
rescue TypeError => e
  puts e.class
end
begin
  (1..3).bsearch { |n| "bad" }
rescue TypeError => e
  puts e.class
end
