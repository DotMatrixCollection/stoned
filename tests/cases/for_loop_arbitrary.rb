# for loop with Hash
for k, v in {a: 1, b: 2, c: 3}
  puts "#{k}=#{v}"
end

# for loop with arbitrary to_a iterable
class Countdown
  def to_a
    [3, 2, 1, 0]
  end
end

for x in Countdown.new
  puts x
end

# for loop with String array
words = %w[hello world]
for w in words
  puts w.upcase
end
