# StopIteration and Enumerator basics

# Manual StopIteration raise/rescue
def first_or_default(arr, default)
  arr.each do |x|
    raise StopIteration if x > 10
    return x
  end
  default
rescue StopIteration
  default
end

puts first_or_default([3, 15, 1], 0)   # 3
puts first_or_default([15, 1, 3], 0)   # 0

# StopIteration is a subclass of IndexError in MRI
# (we accept StandardError as well since we may not have IndexError)
puts StopIteration.ancestors.include?(StandardError)  # true

# loop{} catches StopIteration silently
i = 0
loop do
  raise StopIteration if i >= 3
  i += 1
end
puts i   # 3
