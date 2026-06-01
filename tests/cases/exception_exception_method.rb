begin
  raise ArgumentError, "bad"
rescue ArgumentError => e
  p e.message
  p e.exception.class
  p e.exception("worse").message
end
