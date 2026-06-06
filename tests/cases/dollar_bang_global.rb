# $! is the current exception inside rescue, nil outside
begin
  raise RuntimeError, "test msg"
rescue => e
  puts $!.nil?
  puts $!.class
  puts $!.equal?(e)
  puts $!.message
end
puts $!.nil?

# $! reverts to outer after nested rescue exits
begin
  raise "outer"
rescue => outer_e
  begin
    raise "inner"
  rescue => inner_e
    puts $!.message
  end
  puts $!.message
end
puts $!.nil?
