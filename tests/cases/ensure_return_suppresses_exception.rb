# return from ensure suppresses the propagating exception
def ensure_return_wins
  begin
    raise "should be suppressed"
  ensure
    return "ensure wins"
  end
end
puts ensure_return_wins

# break from ensure suppresses exception and exits block
result = [1].each do |_|
  begin
    raise "suppressed by break"
  ensure
    break "broke from ensure"
  end
end
puts result
