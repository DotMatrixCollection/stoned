class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

s = Seq.new(["zero", "one", "two", "three"])

puts s.find_index("two")
puts s.find_index { |x| x.length == 5 }
puts s.index("missing").inspect
puts s.index { |x| x.start_with?("t") }
puts s.find_index.class
puts({a: 1, b: 2, c: 3}.find_index { |k, v| k == :b && v == 2 })
puts((10..15).find_index(13))
puts((10..15).find_index { |n| n > 20 }.inspect)
puts({a: 1}.respond_to?(:find_index))
puts((1..2).respond_to?(:find_index))
