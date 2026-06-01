result = (1..Float::INFINITY).lazy.select { |n| n.odd? }.first(5)
p result

result2 = (1..Float::INFINITY).lazy.map { |n| n * n }.first(4)
p result2

result3 = (1..Float::INFINITY).lazy.select { |n| n % 3 == 0 }.take(4).to_a
p result3

result4 = [1,2,3,4,5].lazy.reject { |n| n.even? }.map { |n| n * 10 }.to_a
p result4
