begin
  raise ArgumentError, "bad arg"
rescue TypeError, ArgumentError => e
  p e.class
  p e.message
end
begin
  Integer("xyz")
rescue ArgumentError => e
  p e.message.include?("invalid")
end
p RuntimeError.ancestors.include?(StandardError)
