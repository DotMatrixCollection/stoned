# more-specific rescue before less-specific: specific wins
log = []
begin
  raise TypeError, "type problem"
rescue TypeError => e
  log << "type: #{e.message}"
rescue StandardError => e
  log << "standard: #{e.message}"
end
p log

# less-specific first: it matches, more-specific never reached
log2 = []
begin
  raise TypeError, "type problem"
rescue StandardError => e
  log2 << "standard: #{e.message}"
rescue TypeError => e
  log2 << "type: #{e.message}"
end
p log2

# ArgumentError < StandardError: StandardError rescue catches it
begin
  raise ArgumentError, "bad arg"
rescue StandardError => e
  puts "#{e.class}: #{e.message}"
end

# rescue list: first matching class wins
begin
  raise KeyError, "missing key"
rescue ArgumentError, KeyError => e
  puts "matched list: #{e.class}"
end
