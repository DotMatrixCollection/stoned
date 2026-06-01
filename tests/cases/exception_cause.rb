begin
  begin
    raise ArgumentError, "original error"
  rescue => cause
    raise RuntimeError, "wrapped error"
  end
rescue => e
  p e.class
  p e.message
  p e.cause.class
  p e.cause.message
end
