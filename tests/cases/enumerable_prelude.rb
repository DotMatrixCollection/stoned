class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    i = 0
    while i < @items.length
      yield @items[i]
      i = i + 1
    end
  end
end

s = Seq.new([3, 1, 2, 1])

puts s.to_a.inspect
puts s.count
puts s.map { |n| n * 2 }.inspect
puts s.select { |n| n > 1 }.inspect
puts s.reject { |n| n == 1 }.inspect
puts s.reduce { |acc, n| acc + n }
puts s.any? { |n| n == 2 }
puts s.all? { |n| n > 0 }
puts s.none? { |n| n < 0 }
puts s.include?(2)
puts s.member?(9)
puts s.min
puts s.max
puts s.sort.inspect
puts s.sum
puts s.sum(10) { |n| n * 2 }
puts s.flat_map { |n| [n, n + 10] }.inspect
puts s.each_with_object([]) { |n, out| out << (n + 1) }.inspect
puts s.min_by { |n| -n }
puts s.max_by { |n| -n }
puts s.sort_by { |n| -n }.inspect
puts s.zip([:a, :b, :c, :d]).inspect
puts s.group_by { |n| n % 2 }.inspect
puts s.tally.inspect
