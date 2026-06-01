"abc123" =~ /(\d+)/
before = $1
p /z/.match?("abc")
p $1
p before
