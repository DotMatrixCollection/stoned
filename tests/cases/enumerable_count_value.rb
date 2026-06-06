class Nums
  include Enumerable
  def each; [1, 2, 1, 3, 1].each { |n| yield n }; end
end
p Nums.new.count(1)
p Nums.new.count(2)
p Nums.new.count(9)
p Nums.new.count { |n| n.odd? }
