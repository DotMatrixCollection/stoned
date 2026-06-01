begin
  proc
rescue ArgumentError => e
  puts e.message
end

begin
  lambda
rescue ArgumentError => e
  puts e.message
end

begin
  Proc.new
rescue ArgumentError => e
  puts e.message
end
