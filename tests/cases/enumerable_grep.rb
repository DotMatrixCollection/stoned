class Seq
  include Enumerable

  def initialize(items)
    @items = items
  end

  def each
    @items.each { |item| yield item }
  end
end

s = Seq.new(["alpha", "beta", "ace", "zed"])
mixed = Seq.new(["alpha", 7, :sym])

puts s.grep(/^a/).inspect
puts s.grep(/^a/) { |x| x.upcase }.inspect
puts mixed.grep_v(String).inspect
puts s.find_all { |x| x.is_a?(String) && x.length == 3 }.inspect
puts mixed.filter { |x| x.is_a?(Integer) }.inspect
puts [1, 2, 3, 4, 5].grep(2..4).inspect
puts [1, "two", 3.0, :four].grep(Integer).inspect
puts (1..6).grep_v(2..4).inspect
