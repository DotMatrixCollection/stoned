def test_flow(raise_it)
  result = []
  begin
    result << :begin
    raise "error" if raise_it
    result << :no_error
  rescue => e
    result << :rescue
  else
    result << :else
  ensure
    result << :ensure
  end
  result
end

p test_flow(false)
p test_flow(true)
