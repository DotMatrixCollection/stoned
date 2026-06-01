require "strscan"

s = StringScanner.new("abc123def")
p s.check(/[a-z]+/)
p s.pos
p s.skip(/[a-z]+/)
p s.pos
p s.scan_until(/de/)
p s.matched
p s.matched_size
p s.post_match
