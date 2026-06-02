def wrap(value)
  p block_given?
  yield(value + 1)
end

def relay(value, &block)
  wrap(value, &block)
end

relay(4) do |n|
  p n
  p block_given?
end

def no_block
  p block_given?
end

no_block
