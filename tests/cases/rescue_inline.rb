# Inline method-level rescue (no explicit begin)

def safe_divide(a, b)
  a / b
rescue ZeroDivisionError
  :inf
end

puts safe_divide(10, 2)   # 5
puts safe_divide(10, 0)   # inf

# rescue with ensure at method level
def with_ensure(x)
  raise "oops" if x < 0
  x * 2
rescue RuntimeError
  -1
ensure
  # ensure always runs; return value comes from rescue/body
end

puts with_ensure(5)   # 10
puts with_ensure(-1)  # -1

# multiple rescue clauses at method level
def classify(x)
  raise ArgumentError, "negative" if x < 0
  raise TypeError, "too big"      if x > 100
  x
rescue ArgumentError
  :negative
rescue TypeError
  :big
end

puts classify(50)    # 50
puts classify(-1)    # negative
puts classify(200)   # big

# rescue and return value
def risky
  raise ArgumentError, "bad input"
rescue ArgumentError
  0
end

puts risky  # 0
