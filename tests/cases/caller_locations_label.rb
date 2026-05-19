def level1; level2; end
def level2
  loc = caller_locations(0, 1).first
  puts loc.label
  puts loc.lineno > 0
  puts loc.path.is_a?(String)
end
level1
