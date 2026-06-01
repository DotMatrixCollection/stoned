attempts = 0
begin
  attempts += 1
  raise "fail" if attempts < 3
  p "succeeded on attempt #{attempts}"
rescue RuntimeError
  retry if attempts < 3
  p "gave up"
end

def risky
  begin
    raise "oops"
  rescue => e
    return "rescued: #{e.message}"
  ensure
    p "ensure ran"
  end
end
p risky
