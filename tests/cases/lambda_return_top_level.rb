l = -> { return 4 }
puts l.call

def invoke(&blk)
  puts blk.call
end

invoke(&l)
