adder = Proc.new { |x| x + 1 }
puts adder.call(4)
puts adder.lambda?
puts adder.arity

def lambda_demo
  strict = lambda { |x| return x + 2 }
  puts strict.call(5)
  puts strict.lambda?
  puts strict.arity

  begin
    strict.call
  rescue ArgumentError => e
    puts e.class
  end
end

def proc_return
  p = Proc.new { return 7 }
  p.call
  9
end

def lambda_return
  l = lambda { |x| return x + 3 }
  value = l.call(5)
  puts value
  10
end

lambda_demo
puts proc_return
puts lambda_return
