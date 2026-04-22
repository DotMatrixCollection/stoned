def a
  b
end

def b
  raise "boom"
end

begin
  a
rescue => e
  puts e.backtrace.join(" | ")
end
