r = (1..3).zip([4, 5, 6]) { |pair| pair }
p r
collected = []
(1..3).zip([10, 20, 30]) { |a, b| collected << a + b }
p collected
class Nums
  include Enumerable
  def each; [1, 2, 3].each { |n| yield n }; end
end
p Nums.new.zip([4, 5, 6]) { |pair| pair }
