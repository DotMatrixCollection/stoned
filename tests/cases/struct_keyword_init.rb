Point = Struct.new(:x, :y)
Config = Struct.new(:host, :port, keyword_init: true)

puts Point.keyword_init?
puts Config.keyword_init?

p Point[1, 2].to_h
p Config[{:host => "localhost", :port => 3000}].to_h
p Config.new(host: "example.test").to_h

begin
  Config.new("localhost")
rescue => e
  puts e.class
  puts e.message
end

begin
  Config.new(host: "localhost", scheme: "https")
rescue => e
  puts e.class
  puts e.message
end
