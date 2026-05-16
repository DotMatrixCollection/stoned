conf = {}
@conf = {}

def conf.inspect
  "cfg"
end

def @conf.inspect
  "ivar-cfg"
end

puts conf.inspect
puts conf.respond_to?(:inspect)
puts conf.methods.include?(:inspect)
puts conf.method(:inspect).call
puts @conf.inspect
