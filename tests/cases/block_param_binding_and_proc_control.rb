def take_block(&blk)
  puts blk.class
  puts blk.lambda?
end

take_block { 1 }

def make_break_proc
  Proc.new { break 1 }
end

def make_return_proc
  Proc.new { return 2 }
end

def call_block(&blk)
  blk.call
  98
end

begin
  puts call_block(&make_break_proc)
rescue LocalJumpError => e
  puts e.class
end

begin
  puts call_block(&make_return_proc)
rescue LocalJumpError => e
  puts e.class
end
