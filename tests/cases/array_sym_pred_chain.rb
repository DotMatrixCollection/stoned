nums = [2, 4, 6, 8]
p nums.all?(&:even?)
p nums.any?(&:odd?)
p nums.none?(&:odd?)
mixed = [1, 2, 3, 4, 5]
p mixed.any?(&:even?)
p mixed.all? { |n| n > 0 }
p mixed.none? { |n| n > 10 }
