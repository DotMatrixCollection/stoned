class NumberSet
  include Enumerable
  def initialize(*nums); @nums = nums; end
  def each(&block); @nums.each(&block); end
end

ns = NumberSet.new(3, 1, 4, 1, 5, 9)
puts ns.sort.inspect
puts ns.min
puts ns.max
puts ns.select { |n| n > 3 }.sort.inspect
puts ns.map { |n| n * 2 }.inspect
puts ns.sum
puts ns.each_with_index.to_a.inspect
puts ns.each_with_index.map { |v, i| [v, i] }.inspect
puts ns.each_with_object([]) { |n, a| a << n if n.odd? }.sort.inspect
