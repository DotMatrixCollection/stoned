class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

puts Seq.new([nil, false, "x"]).one?
puts Seq.new([1, 2, 3]).one? { |n| n == 2 }
puts Seq.new([1, 2, 3]).one? { |n| n > 1 }
puts Seq.new(["a", 2, :c]).one?(Integer)
puts Seq.new(["a", 2, 3]).one?(Integer)
puts({a: 1, b: 2}.one? { |k, v| k == :a && v == 1 })
puts((1..3).one? { |n| n == 2 })
puts([nil, true, false].respond_to?(:one?))
puts({a: 1}.respond_to?(:one?))
puts((1..2).respond_to?(:one?))
