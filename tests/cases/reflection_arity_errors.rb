begin
  1.class(1)
rescue => e
  puts e.class
end

begin
  nil.nil?(1)
rescue => e
  puts e.class
end

begin
  1.is_a?(Integer, String)
rescue => e
  puts e.class
end

begin
  1.instance_of?(Integer, String)
rescue => e
  puts e.class
end

begin
  1.respond_to?(:to_s, true, false)
rescue => e
  puts e.class
end
