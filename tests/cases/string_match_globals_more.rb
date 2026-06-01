p "item42".match?(/\d+/)
p $~.inspect
p "item42".match(/(\d+)/)[1]
p $1
