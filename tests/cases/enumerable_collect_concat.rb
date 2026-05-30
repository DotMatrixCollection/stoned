class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

seq = Seq.new([1, 2])
puts seq.flat_map { |n| [n, n * 10] }.inspect
puts seq.collect_concat { |n| [n, n * 10] }.inspect
puts [1, 2].collect_concat { |n| [n, n] }.inspect
puts({a: 1, b: 2}.collect_concat { |k, v| [[v, k]] }.inspect)
puts((1..2).collect_concat { |n| [n, n * 2] }.inspect)
puts seq.flat_map.class
puts seq.respond_to?(:collect_concat)
puts [1].respond_to?(:collect_concat)
puts({a: 1}.respond_to?(:collect_concat))
puts((1..2).respond_to?(:collect_concat))
