class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

out = []
Seq.new([:a, :b]).cycle(3) { |x| out << x }
puts out.inspect
puts Seq.new([]).cycle(2) { |x| puts x }.inspect
puts Seq.new([1, 2]).cycle(0) { |x| puts x }.inspect
puts Seq.new([1]).cycle.class
puts [1, 2].respond_to?(:cycle)
puts({a: 1}.respond_to?(:cycle))
puts((1..2).respond_to?(:cycle))
