begin
  raise "boom"
rescue => e
  e.set_backtrace(["custom:1"])
  p e.backtrace
end
