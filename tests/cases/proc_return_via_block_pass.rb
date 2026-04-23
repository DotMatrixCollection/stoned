def invoke(&blk)
  blk.call
  :invoke_after
end

def outer_proc_return
  p = Proc.new { return :from_proc }
  x = invoke(&p)
  [:outer_after, x]
end

def outer_lambda_return
  l = -> { return :from_lambda }
  x = invoke(&l)
  [:outer_after, x]
end

def outer_proc_next
  p = Proc.new { next :from_next }
  x = invoke(&p)
  [:outer_after, x]
end

puts outer_proc_return.inspect
puts outer_lambda_return.inspect
puts outer_proc_next.inspect
