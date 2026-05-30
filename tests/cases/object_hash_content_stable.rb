puts [1, 2, 3].hash == [1, 2, 3].hash
puts [1, 2, 3].hash == [1, 2, 4].hash
puts "hello".hash == "hello".hash
puts "hello".hash == "world".hash
puts :foo.hash == :foo.hash
puts :foo.hash == :bar.hash
puts 42.hash == 42.hash
puts({a: 1, b: 2}.hash == {a: 1, b: 2}.hash)
puts nil.hash == nil.hash
puts true.hash == true.hash
puts false.hash == true.hash

h = {}
h[[1, 2].hash] = "cached"
puts h.key?([1, 2].hash)
puts h.key?([1, 3].hash)
