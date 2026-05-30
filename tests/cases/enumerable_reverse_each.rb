class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

s = Seq.new([1, 2, 3, 4])
out = []
ret = s.reverse_each { |n| out << n }

puts out.inspect
puts ret.class
puts s.reverse_each.class
puts({a: 1, b: 2}.reverse_each.map { |pair| pair[0] }.inspect)
puts((1..3).reverse_each.to_a.inspect)
puts([1, 2].respond_to?(:reverse_each))
puts({a: 1}.respond_to?(:reverse_each))
puts((1..2).respond_to?(:reverse_each))
