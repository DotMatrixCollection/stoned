result = [1,2,3,4,5].each_with_object([]) { |x, acc| acc << x * 2 if x.odd? }
p result

result2 = %w[cat dog bird].each_with_object({}) { |s, h| h[s] = s.length }
p result2

enum = [1,2,3].each_with_object([])
p enum.class
