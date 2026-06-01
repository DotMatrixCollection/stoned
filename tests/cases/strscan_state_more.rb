require "strscan"

s = StringScanner.new("abc")
p s.scan(/a/)
p s.matched?
p s.pre_match
p s.post_match
p s.inspect.include?("StringScanner")
