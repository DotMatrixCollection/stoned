require "json"

begin
  JSON.parse("{bad}")
rescue JSON::ParseError => e
  puts e.class
end
