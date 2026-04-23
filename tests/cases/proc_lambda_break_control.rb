def call_break_inside
  p = Proc.new { break 1 }
  p.call
  99
end

def lambda_break_inside
  l = -> { break 4 }
  x = l.call
  puts x
  96
end

begin
  puts call_break_inside
rescue LocalJumpError => e
  puts e.class
  puts e.message
end

puts lambda_break_inside
