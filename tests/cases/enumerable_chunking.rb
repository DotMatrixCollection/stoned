class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

seq = Seq.new([1, 1, 2, 2, 3])
puts seq.chunk { |n| n }.map { |key, vals| [key, vals.length] }.inspect
puts Seq.new([1, 2, 4, 5, 7]).chunk_while { |a, b| b == a + 1 }.inspect
puts Seq.new([1, 2, 3, 7, 8]).slice_when { |a, b| b > a + 1 }.inspect
puts Seq.new([1, 2, 3, 7, 8]).slice_before { |n| n > 5 }.inspect
puts Seq.new([1, 2, 3, 4, 5]).slice_before(3).inspect
puts seq.chunk.class
puts seq.slice_before.class
puts seq.respond_to?(:chunk)
puts [1].respond_to?(:slice_before)
puts({a: 1}.respond_to?(:chunk_while))
puts((1..2).respond_to?(:slice_when))
