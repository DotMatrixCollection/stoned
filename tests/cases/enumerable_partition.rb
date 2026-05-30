class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

puts Seq.new([1, 2, 3, 4]).partition { |x| x.odd? }.inspect
puts({a: 1, b: 2, c: 3}.partition { |k, v| v == 2 }.inspect)
puts((1..4).partition { |x| x > 2 }.inspect)
puts Seq.new([1]).partition.class
puts Seq.new([1]).respond_to?(:partition)
puts [1, 2].respond_to?(:partition)
puts({a: 1}.respond_to?(:partition))
puts((1..2).respond_to?(:partition))
